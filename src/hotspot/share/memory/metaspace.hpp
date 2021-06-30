/*
 * Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */
#ifndef SHARE_VM_MEMORY_METASPACE_HPP
#define SHARE_VM_MEMORY_METASPACE_HPP

#include "memory/allocation.hpp"
#include "memory/memRegion.hpp"
#include "memory/metaspaceChunkFreeListSummary.hpp"
#include "memory/virtualspace.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/macros.hpp"

// Metaspace
//
// Metaspaces are Arenas for the VM's metadata.
// They are allocated one per class loader object, and one for the null
// bootstrap class loader
//
//    block X ---+       +-------------------+
//               |       |  Virtualspace     |
//               |       |                   |
//               |       |                   |
//               |       |-------------------|
//               |       || Chunk            |
//               |       ||                  |
//               |       ||----------        |
//               +------>||| block 0 |       |
//                       ||----------        |
//                       ||| block 1 |       |
//                       ||----------        |
//                       ||                  |
//                       |-------------------|
//                       |                   |
//                       |                   |
//                       +-------------------+
//

class ClassLoaderData;
class MetaspaceTracer;
class Mutex;
class outputStream;

class CollectedHeap;

namespace metaspace {
  class ChunkManager;
  class ClassLoaderMetaspaceStatistics;
  class Metachunk;
  class PrintCLDMetaspaceInfoClosure;
  class SpaceManager;
  class VirtualSpaceList;
  class VirtualSpaceNode;
}

// Metaspaces each have a  SpaceManager and allocations
// are done by the SpaceManager.  Allocations are done
// out of the current Metachunk.  When the current Metachunk
// is exhausted, the SpaceManager gets a new one from
// the current VirtualSpace.  When the VirtualSpace is exhausted
// the SpaceManager gets a new one.  The SpaceManager
// also manages freelists of available Chunks.
//
// Currently the space manager maintains the list of
// virtual spaces and the list of chunks in use.  Its
// allocate() method returns a block for use as a
// quantum of metadata.

// Namespace for important central static functions
// (auxiliary stuff goes into MetaspaceUtils)
class Metaspace : public AllStatic {

  friend class MetaspaceShared;

 public:
  enum MetadataType {
    ClassType,
    NonClassType,
    MetadataTypeCount
  };
  enum MetaspaceType {
    ZeroMetaspaceType = 0,
    StandardMetaspaceType = ZeroMetaspaceType,
    BootMetaspaceType = StandardMetaspaceType + 1,
    AnonymousMetaspaceType = BootMetaspaceType + 1,
    ReflectionMetaspaceType = AnonymousMetaspaceType + 1,
    MetaspaceTypeCount
  };

 private:

  // Align up the word size to the allocation word size
  static size_t align_word_size_up(size_t);
/*
  // Aligned size of the metaspace.
  static size_t _compressed_class_space_size;

  static size_t compressed_class_space_size() {
    return _compressed_class_space_size;
  }

  static void set_compressed_class_space_size(size_t size) {
    _compressed_class_space_size = size;
  }
*/
  // static size_t _commit_alignment;
  // static size_t _reserve_alignment;
  DEBUG_ONLY(static bool   _frozen;)

  // Virtual Space lists for both classes and other metadata
  static metaspace::VirtualSpaceList* _space_list;
  static metaspace::VirtualSpaceList* _class_space_list;

  static metaspace::ChunkManager* _chunk_manager_metadata;
  static metaspace::ChunkManager* _chunk_manager_class;

  static const MetaspaceTracer* _tracer;

  static bool _initialized;

 public:
  static metaspace::VirtualSpaceList* space_list()       { return _space_list; }
  static metaspace::VirtualSpaceList* class_space_list() { return _class_space_list; }
  static metaspace::VirtualSpaceList* get_space_list(MetadataType mdtype) {
    assert(mdtype != MetadataTypeCount, "MetadaTypeCount can't be used as mdtype");
    return mdtype == ClassType ? class_space_list() : space_list();
  }

  static metaspace::ChunkManager* chunk_manager_metadata() { return _chunk_manager_metadata; }
  static metaspace::ChunkManager* chunk_manager_class()    { return _chunk_manager_class; }
  static metaspace::ChunkManager* get_chunk_manager(MetadataType mdtype) {
    assert(mdtype != MetadataTypeCount, "MetadaTypeCount can't be used as mdtype");
    return mdtype == ClassType ? chunk_manager_class() : chunk_manager_metadata();
  }

  // convenience function
  static metaspace::ChunkManager* get_chunk_manager(bool is_class) {
    return is_class ? chunk_manager_class() : chunk_manager_metadata();
  }

  static const MetaspaceTracer* tracer() { return _tracer; }
  static void freeze() {
    assert(DumpSharedSpaces, "sanity");
    DEBUG_ONLY(_frozen = true;)
  }
  static void assert_not_frozen() {
    assert(!_frozen, "sanity");
  }
#ifdef _LP64
  static void allocate_metaspace_compressed_klass_ptrs(char* requested_addr, address cds_base);
#endif

 private:

#ifdef _LP64
  static void set_narrow_klass_base_and_shift(address metaspace_base, address cds_base);

  // Returns true if can use CDS with metaspace allocated as specified address.
  static bool can_use_cds_with_metaspace_addr(char* metaspace_base, address cds_base);

  static void initialize_class_space(ReservedSpace rs);
#endif

 public:

  static void ergo_initialize();
  static void global_initialize();
  static void post_initialize();

  // Alignment, in bytes, of metaspace mappings
  static size_t reserve_alignment()       { return reserve_alignment_words() * BytesPerWord; }
  // Alignment, in words, of metaspace mappings
  static size_t reserve_alignment_words();

  // The granularity at which Metaspace is committed and uncommitted.
  // (Todo: Why does this have to be exposed?)
  static size_t commit_alignment()        { return commit_alignment_words() * BytesPerWord; }
  static size_t commit_alignment_words();

  // The largest possible single allocation
  static size_t max_allocation_word_size();

  static MetaWord* allocate(ClassLoaderData* loader_data, size_t word_size,
                            MetaspaceObj::Type type, TRAPS);

  static bool contains(const void* ptr);
  static bool contains_non_shared(const void* ptr);

  // Free empty virtualspaces
  // static void purge(MetadataType mdtype);
  static void purge();

  static void report_metadata_oome(ClassLoaderData* loader_data, size_t word_size,
                                   MetaspaceObj::Type type, MetadataType mdtype, TRAPS);

  static const char* metadata_type_name(Metaspace::MetadataType mdtype);

  static void print_compressed_class_space(outputStream* st, const char* requested_addr = 0) NOT_LP64({});

  // Return TRUE only if UseCompressedClassPointers is True.
  static bool using_class_space() {
    return NOT_LP64(false) LP64_ONLY(UseCompressedClassPointers);
  }

  static bool is_class_space_allocation(MetadataType mdType) {
    return mdType == ClassType && using_class_space();
  }

  static bool initialized() { return _initialized; }

};


#endif // SHARE_VM_MEMORY_METASPACE_HPP
