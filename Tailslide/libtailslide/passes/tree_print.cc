#include "tree_print.hh"
#include "../lslmini.hh"

namespace Tailslide {


bool TreePrintingVisitor::visit(LSLASTNode *node) {
  auto lloc = node->getLoc();
  auto type = node->getType();
  auto constant_value = node->getConstantValue();
  int i;
  for (i = 0; i < mWalkLevel; i++) {
    mStream << "  ";
  }
  mStream << node->getNodeName()
          << " [" << (type ? type->getNodeName() : "") << "] "
          << "(cv=" << (constant_value ? constant_value->getNodeName() : "") << ") "
         << '(' << lloc->first_line << ',' << lloc->first_column << ")\n";

  ++mWalkLevel;
  visitChildren(node);
  --mWalkLevel;
  return false;
}

bool TreePrintingVisitor::visit(LSLASTNullNode *node) {
  for (int i = 0; i < mWalkLevel; i++) {
    mStream << "  ";
  }
  mStream << "null\n";
  return false;
}

}
