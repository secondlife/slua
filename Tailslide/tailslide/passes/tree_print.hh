#ifndef TAILSLIDE_TREE_PRINT_HH
#define TAILSLIDE_TREE_PRINT_HH

#include <sstream>

#include "../visitor.hh"

namespace Tailslide {

class TreePrintingVisitor: public ASTVisitor {
  public:
    virtual bool visit(LSLASTNode *node);
    // Don't skip by null or list nodes, they're important parts of the tree we need to show.
    virtual bool visit(LSLASTNullNode *node);
    virtual bool visit(LSLASTNodeList<LSLASTNode> *node) { return visit((LSLASTNode *)node); }

    int mWalkLevel = 0;
    std::stringstream mStream {};
};
}


#endif //TAILSLIDE_TREE_PRINT_HH
