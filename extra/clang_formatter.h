#ifndef CLANG_FORMATTER_H_
#define CLANG_FORMATTER_H_


#ifdef ENABLE_SUPERCFG_DIAG

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Basic/FileSystemOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Parse/Parser.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/MemoryBuffer.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <memory>
#include <sstream>
#include <stack>


class TemplateAwareDiagnosticPrinter : public clang::TextDiagnosticPrinter
{
protected:
    // Custom formatter for template types
    std::string formatTemplateType(const std::string& typeStr) const {
        std::stringstream result;
        std::stack<char> brackets;
        int indentLevel = 0;
        bool newlineNext = false;

        auto addIndent = [&]() {
            result << std::string(indentLevel * 2, ' ');
        };

        for (size_t i = 0; i < typeStr.length(); ++i) {
            char c = typeStr[i];

            if (newlineNext) {
                result << '\n';
                addIndent();
                newlineNext = false;
            }

            switch (c) {
                case '<':
                    result << c;
                    if (i > 0 && (std::isalnum(typeStr[i-1]) ||
                                 typeStr[i-1] == '_' || typeStr[i-1] == '>')) {
                        brackets.push('<');
                        indentLevel++;
                        newlineNext = true;
                    }
                    break;

                case '>':
                    if (!brackets.empty() && brackets.top() == '<') {
                        brackets.pop();
                        if (indentLevel > 0) {
                            indentLevel--;
                            result << '\n';
                            addIndent();
                        }
                    }
                    result << c;
                    break;

                case ',':
                    result << c;
                    if (!brackets.empty()) {
                        newlineNext = true;
                    }
                    break;

                case ' ':
                    // Skip spaces after commas in template contexts
                    if (!brackets.empty() && i > 0 && typeStr[i-1] == ',') {
                        continue;
                    }
                    result << c;
                    break;

                default:
                    result << c;
                    break;
            }
        }

        return result.str();
    }

    // Simplify common verbose template patterns
    std::string simplifyCommonPatterns(const std::string& text) const {
        std::string result = text;

        // Simplify std::integral_constant
        size_t pos = 0;
        while ((pos = result.find("std::integral_constant<", pos)) != std::string::npos) {
            size_t end = pos + 23; // Length of "std::integral_constant<"
            int bracketCount = 1;

            while (end < result.length() && bracketCount > 0) {
                if (result[end] == '<') bracketCount++;
                else if (result[end] == '>') bracketCount--;
                end++;
            }

            if (bracketCount == 0) {
                // Extract the value (assuming it's the last template parameter)
                std::string content = result.substr(pos + 23, end - pos - 24);
                size_t lastComma = content.rfind(',');
                if (lastComma != std::string::npos) {
                    std::string value = content.substr(lastComma + 1);
                    // Trim whitespace
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    result.replace(pos, end - pos, "IC<" + value + ">");
                }
            }
            pos++;
        }

        return result;
    }

public:
    TemplateAwareDiagnosticPrinter(llvm::raw_ostream &os,
                                  clang::DiagnosticOptions& diags,
                                  bool OwnsOutputStream = false)
        : TextDiagnosticPrinter(os, diags, OwnsOutputStream), raw_os(os) {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level Level,
                         const clang::Diagnostic &Info) override {

        // Check if this is a template instantiation error
        bool isTemplateError = false;
        llvm::SmallVector<char> OutStr(256);
        Info.FormatDiagnostic(OutStr);
        std::string diagStr = std::string(OutStr.data());

        // Look for template-related keywords
        if (diagStr.find("instantiation") != std::string::npos ||
            diagStr.find("template") != std::string::npos ||
            diagStr.find("specialization") != std::string::npos) {
            isTemplateError = true;
        }

        if (isTemplateError) {
            // Custom formatting for template errors
            std::string simplified = simplifyCommonPatterns(diagStr);
            std::string formatted = formatTemplateType(simplified);

            // Print our custom formatted version
            raw_os << "\n=== FORMATTED TEMPLATE DIAGNOSTIC ===\n";
            raw_os << formatted << "\n";
            raw_os << "=== END FORMATTED ===\n\n";
        }

        // Always call parent to maintain normal diagnostic flow
        TextDiagnosticPrinter::HandleDiagnostic(Level, Info);
    }

protected:
    llvm::raw_ostream& raw_os;
};



class DecltypeQualTypeExtractor {
private:
    clang::CompilerInstance& CI;
    clang::Sema& sema;
    clang::ASTContext& astContext;

public:
    DecltypeQualTypeExtractor(clang::CompilerInstance& ci)
        : CI(ci), sema(ci.getSema()), astContext(ci.getASTContext()) {}

    // Method 1: Using string-based decltype parsing
    template<typename T>
    clang::QualType getQualTypeFromInstance(T& instance, const std::string& instanceName) {
        // Create a decltype expression string
        std::string decltypeExpr = "decltype(" + instanceName + ")";

        return parseTypeFromString(decltypeExpr);
    }

    // Method 2: More direct approach - parse decltype expression
    clang::QualType parseDecltypeExpression(const std::string& varName) {
        std::string decltypeStr = "decltype(" + varName + ")";
        return parseTypeFromString(decltypeStr);
    }

private:
    clang::QualType parseTypeFromString(const std::string& typeString) {
        // Create a temporary source buffer with the type expression
        std::string tempCode = "auto dummy = " + typeString + "{};";

        // Create memory buffer
        std::unique_ptr<llvm::MemoryBuffer> buffer =
            llvm::MemoryBuffer::getMemBuffer(tempCode, "temp_decltype");

        // Get source manager and create a file ID
        clang::SourceManager& SM = CI.getSourceManager();
        clang::FileID fileID = SM.createFileID(std::move(buffer));

        // Set up lexer
        clang::Lexer lexer(fileID, *SM.getBufferOrNone(fileID), SM, CI.getLangOpts());

        // Parse the type
        clang::Token tok;
        lexer.LexFromRawLexer(tok); // 'auto'
        lexer.LexFromRawLexer(tok); // 'dummy'
        lexer.LexFromRawLexer(tok); // '='
        lexer.LexFromRawLexer(tok); // 'decltype' or first token of type

        // Now we need to parse the decltype expression
        // This is where we integrate with Sema to actually parse and evaluate

        return parseDecltypeFromTokens(fileID);
    }

    clang::QualType parseDecltypeFromTokens(clang::FileID fileID) {
        // Set up parser state
        clang::SourceLocation startLoc = CI.getSourceManager().getLocForStartOfFile(fileID);

        // Create a parser instance
        clang::Parser parser(CI.getPreprocessor(), sema, false);

        // This is simplified - in practice you'd need to properly handle the parsing
        // The key is to parse a decltype-specifier and extract its type

        // For demonstration, return a placeholder
        return astContext.getAutoType(clang::QualType(),
                                     clang::AutoTypeKeyword::DecltypeAuto,
                                     false);
    }
};



// Integration helper class for your parser generator
class SuperCFGDiagnostics {
protected:
    llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diagEngine;
    TemplateAwareDiagnosticPrinter printer;
    clang::DiagnosticOptions diagOpts;
    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs;

    clang::FileSystemOptions fs_opts;
    clang::FileManager file_mgr;
    clang::SourceManager source_mgr;
    clang::CompilerInstance* ci;
    DecltypeQualTypeExtractor dt_extract;
    
public:
    SuperCFGDiagnostics() : diagOpts(), diagIDs(llvm::makeIntrusiveRefCnt<clang::DiagnosticIDs>()), printer(llvm::outs(), diagOpts, false), diagEngine(new clang::DiagnosticsEngine(diagIDs, diagOpts, &printer, false)), file_mgr(fs_opts), source_mgr(*diagEngine, file_mgr), ci(new clang::CompilerInstance()), dt_extract(*ci)
    {}

    // Helper methods for common parser generator diagnostics
    void reportTemplateInstantiationError(clang::SourceLocation loc,
                                        const std::string& templateName,
                                        const std::string& errorMsg) {
        const auto ID = diagEngine->getCustomDiagID(
            clang::DiagnosticsEngine::Error,
            "template instantiation failed for %0: %1");

        auto report = diagEngine->Report(loc, ID);
        report.AddString(templateName);
        report.AddString(errorMsg);
    }

    void reportParsingRuleError(clang::SourceLocation loc,
                              const std::string& ruleName,
                              const std::string& details) {
        const auto ID = diagEngine->getCustomDiagID(
            clang::DiagnosticsEngine::Error,
            "parsing rule %0 failed: %1");

        auto report = diagEngine->Report(loc, ID);
        report.AddString(ruleName);
        report.AddString(details);
    }

    template<unsigned N>
    void reportVerboseTemplateType(clang::SourceLocation loc,
                                 clang::QualType type, const char (&note)[N]) {
        const auto ID = diagEngine->getCustomDiagID(
            clang::DiagnosticsEngine::Note,
            note);

        // Use Clang's type printing with custom policy
        clang::LangOptions langOpts;
        clang::PrintingPolicy policy(langOpts);
        policy.SuppressTagKeyword = false;
        policy.SuppressScope = false;
        policy.SuppressUnwrittenScope = false;
        policy.Bool = true;

        std::string typeStr = type.getAsString(policy);

        diagEngine->Report(loc, ID)
            .AddString(typeStr);
    }

    clang::DiagnosticsEngine& getDiagEngine() { return *diagEngine; }

    // Printers
    template<class TemplateInst, unsigned N>
    void print_template_type(const TemplateInst& inst, const char (&note)[N])
    {
        auto loc = get_dummy_loc();

        const char msg_template[] = ": [SUPERCFG CLANG DIAG] instantiated template type: %0";
        constexpr unsigned M = sizeof(msg_template) / sizeof(char);
        char msg[M + N - 1];
        memcpy(msg, note, N - 1);
        memcpy(msg + N - 1, msg_template, M);

        clang::QualType q_inst = dt_extract.getQualTypeFromInstance(inst, "inst");
        reportVerboseTemplateType(loc, q_inst, msg);
    }

    static inline SuperCFGDiagnostics& get()
    {
        static SuperCFGDiagnostics diag;
        return diag;
    }

protected:
    clang::SourceLocation get_dummy_loc()
    {
        auto file_id = source_mgr.getOrCreateFileID(file_mgr.getVirtualFileRef("generated.cpp", 0, 0), clang::SrcMgr::C_User);
        return source_mgr.getLocForStartOfFile(file_id);
    }
};


#endif // ENABLE_SUPERCFG_DIAG



#endif // CLANG_FORMATTER_H_
