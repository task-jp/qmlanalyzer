#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <QtQml/private/qqmljsengine_p.h>
#include <QtQml/private/qqmljslexer_p.h>
#include <QtQml/private/qqmljsparser_p.h>
#include <QtQml/private/qqmljsastvisitor_p.h>
#include <QtQml/private/qqmljsast_p.h>

#ifdef __GNUC__
#  include <cxxabi.h>
#endif

using namespace QQmlJS;
using namespace QQmlJS::AST;

class QmlAnalizer : public Visitor {
public:
    QmlAnalizer(const QString &source)
        : source(source)
        , lexer(&engine)
        , parser(&engine)
        , ast(nullptr)
        , depth(0)
    {
        lexer.setCode(source, 1);
        if (parser.parse())
            ast = parser.ast();
        else {
            qWarning() << parser.errorMessage();
        }
    }

    void analyze()
    {
        Node::accept(ast, this);
    }

    QByteArray name(const Node *ast) const {
        QByteArray name = typeid(*ast).name();
#ifdef __GNUC__
        name = abi::__cxa_demangle(name.constData(), nullptr, nullptr, nullptr) + 13; // == len(QQmlJS::AST::)
#endif
        return name;
    }

    void terminal(const SourceLocation &token) const {
        if (!token.isValid())
            return;
        qDebug().noquote().nospace() << QString(depth * 2, ' ') << "\"" << source.mid(token.offset, token.length) << "\"";
    }

    void nonterminal(Node *ast) {
        Node::accept(ast, this);
    }

    bool preVisit(Node *ast) override {
        qDebug().noquote().nospace() << QString((depth++) * 2, ' ') << "+ " << name(ast);
        return true;
    }

    void postVisit(Node *ast) override {
        qDebug().noquote().nospace() << QString((--depth) * 2, ' ') << "- " << name(ast);
    }

    bool visit(UiImport *ast) override {
        terminal(ast->importToken);

        if (ast->importUri)
            nonterminal(ast->importUri);
        else
            terminal(ast->fileNameToken);

        terminal(ast->versionToken);
        terminal(ast->asToken);
        terminal(ast->importIdToken);
        terminal(ast->semicolonToken);
        return false;
    }

    bool visit(UiObjectBinding *ast) override {
        if (ast->hasOnToken) {
            nonterminal(ast->qualifiedTypeNameId);
            terminal(ast->colonToken);
            nonterminal(ast->qualifiedId);
        } else {
            nonterminal(ast->qualifiedId);
            terminal(ast->colonToken);
            nonterminal(ast->qualifiedTypeNameId);
        }
        nonterminal(ast->initializer);
        return false;
    }

    bool visit(UiObjectDefinition *ast) override {
        nonterminal(ast->qualifiedTypeNameId);
        nonterminal(ast->initializer);
        return false;
    }

    bool visit(UiObjectInitializer *ast) override {
        terminal(ast->lbraceToken);
        nonterminal(ast->members);
        terminal(ast->rbraceToken);
        return false;
    }

    bool visit(UiScriptBinding *ast) override {
        nonterminal(ast->qualifiedId);
        terminal(ast->colonToken);
        nonterminal(ast->statement);
        return false;
    }

    bool visit(UiArrayBinding *ast) override {
        nonterminal(ast->qualifiedId);
        terminal(ast->colonToken);
        terminal(ast->lbracketToken);
        nonterminal(ast->members);
        terminal(ast->rbracketToken);
        return false;
    }

    bool visit(UiArrayMemberList *ast) override {
        terminal(ast->commaToken);
        nonterminal(ast->member);
        nonterminal(ast->next);
        return false;
    }

    bool visit(UiQualifiedId *ast) override {
        terminal(ast->identifierToken);
        if (ast->next)
            qDebug().noquote().nospace() << QString(depth * 2, ' ') << QString(".");
        nonterminal(ast->next);
        return false;
    }

    bool visit(UiPublicMember *ast) override {
        terminal(ast->defaultToken);
        terminal(ast->readonlyToken);
        terminal(ast->propertyToken);
        terminal(ast->typeModifierToken);
        terminal(ast->typeToken);
        terminal(ast->identifierToken);
        terminal(ast->colonToken);
        nonterminal(ast->statement);
        nonterminal(ast->binding);
        terminal(ast->semicolonToken);
        return false;
    }

    bool visit(StringLiteral *ast) override { terminal(ast->literalToken); return false; }
    bool visit(NumericLiteral *ast) override { terminal(ast->literalToken); return false; }
    bool visit(TrueLiteral *ast) override { terminal(ast->trueToken); return false; }
    bool visit(FalseLiteral *ast) override { terminal(ast->falseToken); return false; }
    bool visit(IdentifierExpression *ast) override { terminal(ast->identifierToken); return false; }
    bool visit(FieldMemberExpression *ast) override { nonterminal(ast->base); terminal(ast->dotToken); terminal(ast->identifierToken); return false; }
    bool visit(BinaryExpression *ast) override { nonterminal(ast->left); terminal(ast->operatorToken); nonterminal(ast->right); return false; }
    bool visit(UnaryPlusExpression *ast) override { terminal(ast->plusToken); nonterminal(ast->expression); return false; }
    bool visit(UnaryMinusExpression *ast) override { terminal(ast->minusToken); nonterminal(ast->expression); return false; }
    bool visit(NestedExpression *ast) override { terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); return false; }
    bool visit(ThisExpression *ast) override { terminal(ast->thisToken); return false; }
    bool visit(NullExpression *ast) override { terminal(ast->nullToken); return false; }
    bool visit(RegExpLiteral *ast) override { terminal(ast->literalToken); return false; }
    bool visit(ArrayPattern *ast) override { terminal(ast->lbracketToken); nonterminal(ast->elements); terminal(ast->commaToken); terminal(ast->rbracketToken); return false; }
    bool visit(ObjectPattern *ast) override { terminal(ast->lbraceToken); nonterminal(ast->properties); terminal(ast->rbraceToken); return false; }
    bool visit(PatternElementList *ast) override { nonterminal(ast->next); nonterminal(ast->elision); nonterminal(ast->element->initializer); return false; }
    bool visit(Elision *ast) override { nonterminal(ast->next); terminal(ast->commaToken); return false; }
    bool visit(PatternPropertyList *ast) override { nonterminal(AST::cast<AST::PatternProperty *>(ast->property)); nonterminal(ast->next); return false; }
    bool visit(IdentifierPropertyName *ast) override { terminal(ast->propertyNameToken); return false; }
    bool visit(StringLiteralPropertyName *ast) override { terminal(ast->propertyNameToken); return false; }
    bool visit(NumericLiteralPropertyName *ast) override { terminal(ast->propertyNameToken); return false; }
    bool visit(ArrayMemberExpression *ast) override { nonterminal(ast->base); terminal(ast->lbracketToken); nonterminal(ast->expression); terminal(ast->rbracketToken); return false; }
    bool visit(NewMemberExpression *ast) override { terminal(ast->newToken); nonterminal(ast->base); terminal(ast->lparenToken); nonterminal(ast->arguments); terminal(ast->rparenToken); return false; }
    bool visit(NewExpression *ast) override { terminal(ast->newToken); nonterminal(ast->expression); return false; }
    bool visit(CallExpression *ast) override { nonterminal(ast->base); terminal(ast->lparenToken); nonterminal(ast->arguments); terminal(ast->rparenToken); return false; }
    bool visit(ArgumentList *ast) override { nonterminal(ast->expression); terminal(ast->commaToken); nonterminal(ast->next); return false; }
    bool visit(PostIncrementExpression *ast) override { nonterminal(ast->base); terminal(ast->incrementToken); return false; }
    bool visit(PostDecrementExpression *ast) override { nonterminal(ast->base); terminal(ast->decrementToken); return false; }
    bool visit(DeleteExpression *ast) override { terminal(ast->deleteToken); nonterminal(ast->expression); return false; }
    bool visit(VoidExpression *ast) override { terminal(ast->voidToken); nonterminal(ast->expression); return false; }
    bool visit(TypeOfExpression *ast) override { terminal(ast->typeofToken); nonterminal(ast->expression); return false; }
    bool visit(PreIncrementExpression *ast) override { terminal(ast->incrementToken); nonterminal(ast->expression); return false; }
    bool visit(PreDecrementExpression *ast) override { terminal(ast->decrementToken); nonterminal(ast->expression); return false; }
    bool visit(TildeExpression *ast) override { terminal(ast->tildeToken); nonterminal(ast->expression); return false; }
    bool visit(NotExpression *ast) override { terminal(ast->notToken); nonterminal(ast->expression); return false; }
    bool visit(ConditionalExpression *ast) override { nonterminal(ast->expression); terminal(ast->questionToken); nonterminal(ast->ok); terminal(ast->colonToken); nonterminal(ast->ko); return false; }
    bool visit(Expression *ast) override { nonterminal(ast->left); terminal(ast->commaToken); nonterminal(ast->right); return false; }
    bool visit(Block *ast) override { terminal(ast->lbraceToken); nonterminal(ast->statements); terminal(ast->rbraceToken); return false; }
    bool visit(VariableStatement *ast) override { terminal(ast->declarationKindToken); nonterminal(ast->declarations); return false; }
    //bool visit(VariableDeclaration *ast) override { terminal(ast->identifierToken); nonterminal(ast->expression); return false; }
    bool visit(VariableDeclarationList *ast) override { nonterminal(ast->declaration); terminal(ast->commaToken); nonterminal(ast->next); return false; }
    bool visit(EmptyStatement* ast) override { terminal(ast->semicolonToken); return false; }
    bool visit(ExpressionStatement *ast) override { nonterminal(ast->expression); terminal(ast->semicolonToken); return false; }
    bool visit(IfStatement *ast) override { terminal(ast->ifToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->ok); terminal(ast->elseToken); nonterminal(ast->ko); return false; }
    bool visit(DoWhileStatement *ast) override { terminal(ast->doToken); nonterminal(ast->statement); terminal(ast->whileToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); terminal(ast->semicolonToken); return false; }
    bool visit(WhileStatement *ast) override { terminal(ast->whileToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->statement); return false; }
    bool visit(ForStatement *ast) override { terminal(ast->forToken); terminal(ast->lparenToken); nonterminal(ast->initialiser); terminal(ast->firstSemicolonToken); nonterminal(ast->condition); terminal(ast->secondSemicolonToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->statement); return false; }
    bool visit(ForEachStatement *ast) override { terminal(ast->forToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->statement); return false; }
    bool visit(ContinueStatement *ast) override { terminal(ast->continueToken); return false; }
    bool visit(BreakStatement *ast) override { terminal(ast->breakToken); return false; }
    bool visit(ReturnStatement *ast) override { terminal(ast->returnToken); nonterminal(ast->expression); return false; }
    bool visit(WithStatement *ast) override { terminal(ast->withToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->statement); return false; }
    bool visit(CaseBlock *ast) override { terminal(ast->lbraceToken); nonterminal(ast->clauses); nonterminal(ast->defaultClause); nonterminal(ast->moreClauses); terminal(ast->rbraceToken); return false; }
    bool visit(SwitchStatement *ast) override { terminal(ast->switchToken); terminal(ast->lparenToken); nonterminal(ast->expression); terminal(ast->rparenToken); nonterminal(ast->block); return false; }
    bool visit(CaseClause *ast) override { terminal(ast->caseToken); nonterminal(ast->expression); terminal(ast->colonToken); nonterminal(ast->statements); return false; }
    bool visit(DefaultClause *ast) override { terminal(ast->defaultToken); terminal(ast->colonToken); nonterminal(ast->statements); return false; }
    bool visit(LabelledStatement *ast) override { terminal(ast->identifierToken); terminal(ast->colonToken); nonterminal(ast->statement); return false; }
    bool visit(ThrowStatement *ast) override { terminal(ast->throwToken); nonterminal(ast->expression); return false; }
    bool visit(Catch *ast) override { terminal(ast->catchToken); terminal(ast->lparenToken); terminal(ast->identifierToken); terminal(ast->rparenToken); nonterminal(ast->statement); return false; }
    bool visit(Finally *ast) override { terminal(ast->finallyToken); nonterminal(ast->statement); return false; }
    bool visit(FunctionExpression *ast) override { terminal(ast->functionToken); terminal(ast->identifierToken); terminal(ast->lparenToken); nonterminal(ast->formals); terminal(ast->rparenToken); terminal(ast->lbraceToken); nonterminal(ast->body); terminal(ast->rbraceToken); return false; }
    bool visit(FunctionDeclaration *ast) override { return visit(static_cast<FunctionExpression *>(ast)); }
    bool visit(DebuggerStatement *ast) override { terminal(ast->debuggerToken); terminal(ast->semicolonToken); return false; }
    bool visit(UiParameterList *ast) override { terminal(ast->commaToken); terminal(ast->identifierToken); nonterminal(ast->next); return false; }

#if QT_VERSION >= QT_VERSION_CHECK(5,11,1)
    void throwRecursionDepthError() override {}
#endif

private:
    QString source;
    Engine engine;
    Lexer lexer;
    Parser parser;
    QQmlJS::AST::Node *ast;
    int depth;
};


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString source("import QtQuick 2.0\nItem { id: root }");
    if (argc > 1) {
        QFile file(argv[1]);
        if (file.open(QFile::ReadOnly)) {
            source = QString::fromUtf8(file.readAll());
            file.close();
        } else {
            qWarning() << file.errorString();
        }
    }
    QmlAnalizer analizer(source);
    analizer.analyze();

    return 0;
}
