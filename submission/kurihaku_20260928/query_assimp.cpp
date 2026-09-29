#include <assimp/version.h>
#include <iostream>
int main() {
    std::cout << aiGetVersionMajor() << '.' << aiGetVersionMinor() << '.' << aiGetVersionPatch()
              << " revision=" << aiGetVersionRevision() << " branch=" << aiGetBranchName()
              << " flags=" << aiGetCompileFlags() << '\n' << aiGetLegalString() << '\n';
}
