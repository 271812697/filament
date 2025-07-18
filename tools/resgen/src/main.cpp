/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/Path.h>

#include <getopt/getopt.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <string.h>

using namespace std;
using namespace utils;

static const char* g_jsonMagicString = "__RESGEN__";
static const char* g_packageName = "resources";
static const char* g_deployDir = ".";
static bool g_keepExtension = false;
static bool g_appendNull = false;
static bool g_generateC = false;
static bool g_quietMode = false;
static bool g_embedJson = false;

static const char* USAGE = R"TXT(
RESGEN aggregates a sequence of binary blobs, each of which becomes a "resource" whose id
is the basename of the input file. It produces the following set of files:

    resources.h ......... declares sizes and offsets for each resource
    resources.S ......... small assembly file with incbin directive and rodata section
    resources.apple.S ... ditto but with different rodata name and underscore prefixes
    resources.bin ....... the aggregated binary blob that the incbin refers to

Usage:
    RESGEN [options] <input_file_0> <input_file_1> ...

Options:
   --help, -h
       Print this message
   --license, -L
       Print copyright and license information
   --package=string, -p string
       Name of the resource package (defaults to "resources")
       This is used to generate filenames and symbol prefixes
   --deploy=dir, -x dir (defaults to ".")
       Generate everything needed for deployment into <dir>
   --keep, -k
       Keep file extensions when generating symbols
   --text, -t
       Append a null terminator to each data blob
   --cfile, -c
       Generate xxd-style C file (useful for WebAssembly)
   --json, -j
       Embed a JSON string in the output that provides a summary
       of all resource sizes and names. Useful for size analysis.
    --quiet, -q
        Suppress console output

Examples:
    RESGEN -cp textures jungle.png beach.png
    > Generated files: textures.h, textures.S, textures.apple.S, textures.bin, textures.c
    > Generated symbols: TEXTURES_JUNGLE_DATA, TEXTURES_JUNGLE_SIZE,
                         TEXTURES_BEACH_DATA, TEXTURES_BEACH_SIZE
)TXT";

static const char* APPLE_ASM_TEMPLATE = R"ASM(
    .global _{RESOURCES}PACKAGE
    .section __TEXT,__const
_{RESOURCES}PACKAGE:
    .incbin "{resources}.bin"
)ASM";

static const char* ASM_TEMPLATE = R"ASM(
    .global {RESOURCES}PACKAGE
    .section .rodata
{RESOURCES}PACKAGE:
    .incbin "{resources}.bin"
)ASM";

static void printUsage(const char* name) {
    std::string execName(Path(name).getName());
    const std::string from("RESGEN");
    std::string usage(USAGE);
    for (size_t pos = usage.find(from); pos != std::string::npos; pos = usage.find(from, pos)) {
        usage.replace(pos, from.length(), execName);
    }
    puts(usage.c_str());
}

static void license() {
    static const char *license[] = {
        #include "licenses/licenses.inc"
        nullptr
    };

    const char **p = &license[0];
    while (*p)
        std::cout << *p++ << std::endl;
}

static int handleArguments(int argc, char* argv[]) {
    static constexpr const char* OPTSTR = "hLp:x:ktcqj";
    static const struct option OPTIONS[] = {
            { "help",                 no_argument, 0, 'h' },
            { "license",              no_argument, 0, 'L' },
            { "package",        required_argument, 0, 'p' },
            { "deploy",         required_argument, 0, 'x' },
            { "keep",                 no_argument, 0, 'k' },
            { "text",                 no_argument, 0, 't' },
            { "cfile",                no_argument, 0, 'c' },
            { "quiet",                no_argument, 0, 'q' },
            { "json",                 no_argument, 0, 'j' },
            { 0, 0, 0, 0 }  // termination of the option list
    };

    int opt;
    int optionIndex = 0;

    while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &optionIndex)) >= 0) {
        std::string arg(optarg ? optarg : "");
        switch (opt) {
            default:
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case 'L':
                license();
                exit(0);
            case 'p':
                g_packageName = optarg;
                break;
            case 'x':
                g_deployDir = optarg;
                break;
            case 'k':
                g_keepExtension = true;
                break;
            case 't':
                g_appendNull = true;
                break;
            case 'c':
                g_generateC = true;
                break;
            case 'q':
                g_quietMode = true;
                break;
            case 'j':
                g_embedJson = true;
                break;
        }
    }

    return optind;
}

int main(int argc, char* argv[]) {
    const int optionIndex = handleArguments(argc, argv);
    const int numArgs = argc - optionIndex;
    if (numArgs < 1) {
        printUsage(argv[0]);
        return 1;
    }

    vector<Path> inputPaths;
    for (int argIndex = optionIndex; argIndex < argc; ++argIndex) {
        inputPaths.emplace_back(argv[argIndex]);
    }
    if (g_embedJson) {
        inputPaths.push_back(g_jsonMagicString);
    }

    std::string packageFile = g_packageName;
    std::string packagePrefix = std::string(g_packageName) + "_";
    transform(packageFile.begin(), packageFile.end(), packageFile.begin(), ::tolower);
    transform(packagePrefix.begin(), packagePrefix.end(), packagePrefix.begin(), ::toupper);
    std::string package = packagePrefix + "PACKAGE";

    const Path deployDir(g_deployDir);
    if (!deployDir.exists()) {
        deployDir.mkdirRecursive();
    }

    const Path appleAsmPath(deployDir + (packageFile + ".apple.S"));
    const Path asmPath(deployDir + (packageFile + ".S"));
    const Path binPath(deployDir + (packageFile + ".bin"));
    const Path headerPath(deployDir + (packageFile + ".h"));
    const Path xxdPath(deployDir + (packageFile + ".c"));

    // In the assembly language templates, replace {RESOURCES} with packagePrefix and replace
    // {resources} with packageFile.
    const std::string k1("{RESOURCES}");
    const std::string k2("{resources}");
    std::string aasmstr(APPLE_ASM_TEMPLATE);
    std::string asmstr(ASM_TEMPLATE);
    for (size_t pos = aasmstr.find(k1); pos != std::string::npos; pos = aasmstr.find(k1, pos))
        aasmstr.replace(pos, k1.length(), packagePrefix);
    for (size_t pos = aasmstr.find(k2); pos != std::string::npos; pos = aasmstr.find(k2, pos))
        aasmstr.replace(pos, k2.length(), packageFile);
    for (size_t pos = asmstr.find(k1); pos != std::string::npos; pos = asmstr.find(k1, pos))
        asmstr.replace(pos, k1.length(), packagePrefix);
    for (size_t pos = asmstr.find(k2); pos != std::string::npos; pos = asmstr.find(k2, pos))
        asmstr.replace(pos, k2.length(), packageFile);

    // Open the Apple-friendly assembly language file.
    ofstream appleAsmStream(appleAsmPath.getPath());
    if (!appleAsmStream) {
        cerr << "Unable to open " << appleAsmPath << endl;
        exit(1);
    }

    // Open the non-Apple assembly language file.
    ofstream asmStream(asmPath.getPath());
    if (!asmStream) {
        cerr << "Unable to open " << asmPath << endl;
        exit(1);
    }

    // Open the bin file for writing.
    ofstream binStream(binPath.getPath(), ios::binary);
    if (!binStream) {
        cerr << "Unable to open " << binPath << endl;
        exit(1);
    }

    // Open the header file stream for writing.
    ostringstream headerStream;
    headerStream << "#ifndef " << packagePrefix << "H_" << endl
            << "#define " << packagePrefix << "H_" << endl << endl
            << "#include <stdint.h>\n" << endl
            << "extern \"C\" {" << endl
            << "    extern const uint8_t " << package << "[];" << endl;

    ostringstream headerMacros;
    ostringstream appleDataAsmStream;
    ostringstream dataAsmStream;
    ostringstream jsonStream;

    // Open the generated C file for writing.
    ofstream xxdStream;
    if (g_generateC) {
        xxdStream = ofstream(xxdPath.getPath());
        if (!xxdStream) {
            cerr << "Unable to open " << xxdPath << endl;
            exit(1);
        }
        xxdStream << "#include <stdint.h>\n"
                  << "const uint8_t " << package << "[] = {\n";
    }

    // Consume each input file and write it back out into the various output streams.
    jsonStream << "{";
    size_t offset = 0;
    for (const auto& inPath : inputPaths) {
        vector<uint8_t> content;
        if (inPath != g_jsonMagicString) {
            ifstream inStream(inPath.getPath(), ios::binary);
            if (!inStream) {
                cerr << "Unable to open " << inPath << endl;
                exit(1);
            }
            content = vector<uint8_t>((istreambuf_iterator<char>(inStream)), {});
        } else {
            // To finalize the JSON string, we replace the trailing comma with an end bracket and
            // prefix it with the magic identifier and string size.
            std::string jsonString = jsonStream.str();
            jsonString[jsonString.size()-1] = '}';
            ostringstream jsonBlob;
            jsonBlob << g_jsonMagicString << "\0";
            jsonBlob << jsonString.size() << "\0";
            jsonBlob << jsonString;
            jsonString = jsonBlob.str();
            uint8_t const* jsonPtr = (uint8_t const*) jsonString.c_str();
            content = vector<uint8_t>(jsonPtr, jsonPtr + jsonBlob.str().size());
        }
        if (g_appendNull) {
            content.push_back(0);
        }

        // Formulate the resource name and the prefixed resource name.
        std::string rname = g_keepExtension ? inPath.getName() : inPath.getNameWithoutExtension();
        replace(rname.begin(), rname.end(), '.', '_');
        transform(rname.begin(), rname.end(), rname.begin(), ::toupper);
        const std::string prname = packagePrefix + rname;

        // Write the binary blob into the bin file.
        binStream.write((const char*) content.data(), content.size());

        // Write the offsets and sizes.
        headerMacros
                << "#define " << prname << "_OFFSET " << offset << "\n"
                << "#define " << prname << "_SIZE " << content.size() << "\n"
                << "#define " << prname << "_DATA (" << package << " + " << prname << "_OFFSET)\n\n";

        // Write the xxd-style ASCII array, followed by a blank line.
        if (g_generateC) {
            xxdStream << "// " << rname << "\n";
            xxdStream << setfill('0') << hex;
            size_t i = 0;
            for (; i < content.size(); i++) {
                if (i > 0 && i % 20 == 0) {
                    xxdStream << "\n";
                }
                xxdStream << "0x" << setw(2) << (int) content[i] << ", ";
            }
            if (i % 20 != 0) xxdStream << "\n";
            xxdStream << "\n";
        }

        jsonStream << "\"" << rname << "\":" << content.size() << ",";
        offset += content.size();
    }

    headerStream << "}\n\n";
    headerStream << headerMacros.str();
    headerStream << "#endif\n";

    // To optimize builds, avoid overwriting the header file if nothing has changed.
    bool headerIsDirty = true;
    ifstream headerInStream(headerPath.getPath(), std::ifstream::ate);
    string headerContents = headerStream.str();
    if (headerInStream) {
        long fileSize = static_cast<long>(headerInStream.tellg());
        if (fileSize == headerContents.size()) {
            vector<char> previous(fileSize);
            headerInStream.seekg(0);
            headerInStream.read(previous.data(), fileSize);
            headerIsDirty = 0 != memcmp(previous.data(), headerContents.c_str(), fileSize);
        }
    }

    if (headerIsDirty) {
        ofstream headerOutStream(headerPath.getPath());
        if (!headerOutStream) {
            cerr << "Unable to open " << headerPath << endl;
            exit(1);
        }
        headerOutStream << headerContents;
    }

    asmStream << asmstr << dataAsmStream.str() << endl;
    asmStream.close();

    appleAsmStream << aasmstr << appleDataAsmStream.str() << endl;
    appleAsmStream.close();

    if (!g_quietMode) {
        cout << "Generated files: "
            << headerPath << " "
            << asmPath << " "
            << appleAsmPath << " "
            << binPath;
    }

    if (g_generateC) {
        xxdStream << "};\n\n";
        if (!g_quietMode) {
            cout << " " << xxdPath;
        }
    }

    if (!g_quietMode) {
        cout << endl;
    }
}
