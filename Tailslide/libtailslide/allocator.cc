#include "allocator.hh"

namespace Tailslide {

ScriptAllocator::~ScriptAllocator() {
  for(auto &obj_ptr : _mTrackedObjects) {
    delete obj_ptr;
  }
  for(auto &obj_ptr : _mMallocs) {
    free(obj_ptr);
  }
}

}
