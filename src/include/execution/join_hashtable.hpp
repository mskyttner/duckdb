//===----------------------------------------------------------------------===//
//                         DuckDB
//
// execution/join_hashtable.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/common.hpp"
#include "common/types/data_chunk.hpp"
#include "common/types/tuple.hpp"
#include "common/types/vector.hpp"
#include "planner/operator/logical_join.hpp"

#include <mutex>

namespace duckdb {

//! JoinHashTable is a linear probing HT that is used for computing joins
/*!
   The JoinHashTable concatenates incoming chunks inside a linked list of
   data ptrs. The storage looks like this internally.
   [SERIALIZED ROW][NEXT POINTER]
   [SERIALIZED ROW][NEXT POINTER]
   There is a separate hash map of pointers that point into this table.
   This is what is used to resolve the hashes.
   [POINTER]
   [POINTER]
   [POINTER]
   The pointers are either NULL
*/
class JoinHashTable {
public:
	//! Scan structure that can be used to resume scans, as a single probe can
	//! return 1024*N values (where N is the size of the HT). This is
	//! returned by the JoinHashTable::Scan function and can be used to resume a
	//! probe.
	struct ScanStructure {
		Vector pointers;
		Vector build_pointer_vector;
		sel_t sel_vector[STANDARD_VECTOR_SIZE];
		Tuple serialized_keys[STANDARD_VECTOR_SIZE];
		// whether or not the given tuple has found a match, used for LeftJoin
		bool found_match[STANDARD_VECTOR_SIZE];
		JoinHashTable &ht;
		bool finished;

		ScanStructure(JoinHashTable &ht);
		//! Get the next batch of data from the scan structure
		void Next(DataChunk &keys, DataChunk &left, DataChunk &result);

	private:
		//! Next operator for the inner join
		void NextInnerJoin(DataChunk &keys, DataChunk &left, DataChunk &result);
		//! Next operator for the semi join
		void NextSemiJoin(DataChunk &keys, DataChunk &left, DataChunk &result);
		//! Next operator for the anti join
		void NextAntiJoin(DataChunk &keys, DataChunk &left, DataChunk &result);
		//! Next operator for the left outer join
		void NextLeftJoin(DataChunk &keys, DataChunk &left, DataChunk &result);

		template <bool MATCH> void NextSemiOrAntiJoin(DataChunk &keys, DataChunk &left, DataChunk &result);

		void ResolvePredicates(DataChunk &keys, Vector &comparison_result);
	};

private:
	//! Nodes store the actual data of the tuples inside the HT as a linked list
	struct Node {
		size_t count;
		size_t capacity;
		unique_ptr<uint8_t[]> data;
		unique_ptr<Node> prev;

		Node(size_t tuple_size, size_t capacity) : count(0), capacity(capacity) {
			data = unique_ptr<uint8_t[]>(new uint8_t[tuple_size * capacity]);
			memset(data.get(), 0, tuple_size * capacity);
		}
		~Node() {
			if (prev) {
				auto current_prev = move(prev);
				while (current_prev) {
					auto next_node = move(current_prev->prev);
					current_prev = move(next_node);
				}
			}
		}
	};

	void Hash(DataChunk &keys, Vector &hashes);

public:
	JoinHashTable(vector<JoinCondition> &conditions, vector<TypeId> build_types, JoinType type,
	              size_t initial_capacity = 32768, bool parallel = false);
	//! Resize the HT to the specified size. Must be larger than the current
	//! size.
	void Resize(size_t size);
	//! Add the given data to the HT
	void Build(DataChunk &keys, DataChunk &input);
	//! Probe the HT with the given input chunk, resulting in the given result
	unique_ptr<ScanStructure> Probe(DataChunk &keys);

	//! The stringheap of the JoinHashTable
	StringHeap string_heap;

	size_t size() {
		return count;
	}

	//! Serializer for the keys used in equality comparison
	TupleSerializer equality_serializer;
	//! Serializer for all conditions
	TupleSerializer condition_serializer;
	//! Serializer for the build side
	TupleSerializer build_serializer;
	//! The types of the keys used in equality comparison
	vector<TypeId> equality_types;
	//! The types of the keys
	vector<TypeId> condition_types;
	//! The types of all conditions
	vector<TypeId> build_types;
	//! The comparison predicates
	vector<ExpressionType> predicates;
	//! Size of condition keys
	size_t equality_size;
	//! Size of condition keys
	size_t condition_size;
	//! Size of build tuple
	size_t build_size;
	//! The size of an entry as stored in the HashTable
	size_t entry_size;
	//! The total tuple size
	size_t tuple_size;
	//! The join type of the HT
	JoinType join_type;

private:
	//! Insert the given set of locations into the HT with the given set of
	//! hashes. Caller should hold lock in parallel HT.
	void InsertHashes(Vector &hashes, uint8_t *key_locations[]);
	//! The capacity of the HT. This can be increased using
	//! JoinHashTable::Resize
	size_t capacity;
	//! The amount of entries stored in the HT currently
	size_t count;
	//! The data of the HT
	unique_ptr<Node> head;
	//! The hash map of the HT
	unique_ptr<uint8_t *[]> hashed_pointers;
	//! Whether or not the HT has to support parallel build
	bool parallel = false;
	//! Mutex used for parallelism
	std::mutex parallel_lock;

	//! Copying not allowed
	JoinHashTable(const JoinHashTable &) = delete;
};

} // namespace duckdb
