#include "test_helper.h"
#include "helpers/tree_helpers.h"
#include "helpers/point_helpers.h"
#include "helpers/record_alloc.h"
#include "helpers/stream_methods.h"
#include "runtime/stack.h"
#include "runtime/subtree.h"
#include "runtime/length.h"
#include "runtime/alloc.h"

enum {
  stateA = 2,
  stateB,
  stateC, stateD, stateE, stateF, stateG, stateH, stateI, stateJ
};

enum {
  symbol0, symbol1, symbol2, symbol3, symbol4, symbol5, symbol6, symbol7, symbol8,
  symbol9, symbol10
};

void free_slice_array(SubtreePool *pool, StackSliceArray *slices) {
  for (size_t i = 0; i < slices->size; i++) {
    StackSlice slice = slices->contents[i];

    bool matches_prior_trees = false;
    for (size_t j = 0; j < i; j++) {
      StackSlice prior_slice = slices->contents[j];
      if (slice.subtrees.contents == prior_slice.subtrees.contents) {
        matches_prior_trees = true;
        break;
      }
    }

    if (!matches_prior_trees) {
      for (size_t j = 0; j < slice.subtrees.size; j++)
        ts_subtree_release(pool, slice.subtrees.contents[j]);
      array_delete(&slice.subtrees);
    }
  }
}

SubtreeHeapData *mutate(Subtree subtree) {
  return ts_subtree_to_mut_unsafe(subtree).ptr;
}

struct StackEntry {
  TSStateId state;
  size_t depth;
};

vector<StackEntry> get_stack_entries(Stack *stack, StackVersion version) {
  vector<StackEntry> result;
  ts_stack_iterate(
    stack,
    version,
    [](void *payload, TSStateId state, uint32_t subtree_count) {
      auto entries = static_cast<vector<StackEntry> *>(payload);
      StackEntry entry = {state, subtree_count};
      if (find(entries->begin(), entries->end(), entry) == entries->end()) {
        entries->push_back(entry);
      }
    }, &result);
  return result;
}

START_TEST

describe("Stack", [&]() {
  Stack *stack;
  const size_t subtree_count = 11;
  Subtree subtrees[subtree_count];
  Length tree_len = {3, {0, 3}};
  SubtreePool pool;

  before_each([&]() {
    record_alloc::start();

    pool = ts_subtree_pool_new(10);
    stack = ts_stack_new(&pool);

    TSLanguage dummy_language;
    TSSymbolMetadata symbol_metadata[50] = {};
    dummy_language.symbol_metadata = symbol_metadata;

    for (size_t i = 0; i < subtree_count; i++) {
      subtrees[i] = ts_subtree_new_leaf(
        &pool, i + 1, length_zero(), tree_len, 0,
        TS_TREE_STATE_NONE, true, false, &dummy_language
      );
      ts_external_scanner_state_init(&mutate(subtrees[i])->external_scanner_state, nullptr, 0);
    }
  });

  after_each([&]() {
    ts_stack_delete(stack);
    for (size_t i = 0; i < subtree_count; i++) {
      ts_subtree_release(&pool, subtrees[i]);
    }
    ts_subtree_pool_delete(&pool);

    record_alloc::stop();
    AssertThat(record_alloc::outstanding_allocation_indices(), IsEmpty());
  });

  auto push = [&](StackVersion version, Subtree tree, TSStateId state) {
    ts_subtree_retain(tree);
    ts_stack_push(stack, version, tree, false, state);
  };

  describe("push(version, tree, is_pending, state)", [&]() {
    it("adds entries to the given version of the stack", [&]() {
      AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
      AssertThat(ts_stack_state(stack, 0), Equals(1));
      AssertThat(ts_stack_position(stack, 0), Equals(length_zero()));

      // . <──0── A*
      push(0, subtrees[0], stateA);
      AssertThat(ts_stack_state(stack, 0), Equals(stateA));
      AssertThat(ts_stack_position(stack, 0), Equals(tree_len));

      // . <──0── A <──1── B*
      push(0, subtrees[1], stateB);
      AssertThat(ts_stack_state(stack, 0), Equals(stateB));
      AssertThat(ts_stack_position(stack, 0), Equals(tree_len * 2));

      // . <──0── A <──1── B <──2── C*
      push(0, subtrees[2], stateC);
      AssertThat(ts_stack_state(stack, 0), Equals(stateC));
      AssertThat(ts_stack_position(stack, 0), Equals(tree_len * 3));

      AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
        {stateC, 0},
        {stateB, 1},
        {stateA, 2},
        {1, 3},
      })));
    });
  });

  describe("merge()", [&]() {
    before_each([&]() {
      // . <──0── A <─*
      //          ↑
      //          └───*
      push(0, subtrees[0], stateA);
      ts_stack_copy_version(stack, 0);
    });

    it("combines versions that have the same top states and positions", [&]() {
      // . <──0── A <──1── B <──3── D*
      //          ↑
      //          └───2─── C <──4── D*
      push(0, subtrees[1], stateB);
      push(1, subtrees[2], stateC);
      push(0, subtrees[3], stateD);
      push(1, subtrees[4], stateD);

      // . <──0── A <──1── B <──3── D*
      //          ↑                 |
      //          └───2─── C <──4───┘
      AssertThat(ts_stack_merge(stack, 0, 1), IsTrue());
      AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
      AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
        {stateD, 0},
        {stateB, 1},
        {stateC, 1},
        {stateA, 2},
        {1, 3},
      })));
    });

    it("does not combine versions that have different states", [&]() {
      // . <──0── A <──1── B*
      //          ↑
      //          └───2─── C*
      push(0, subtrees[1], stateB);
      push(1, subtrees[2], stateC);

      AssertThat(ts_stack_merge(stack, 0, 1), IsFalse());
      AssertThat(ts_stack_version_count(stack), Equals<size_t>(2));
    });

    it("does not combine versions that have different positions", [&]() {
      // . <──0── A <──1── B <────3──── D*
      //          ↑
      //          └───2─── C <──4── D*
      mutate(subtrees[3])->size = tree_len * 3;
      push(0, subtrees[1], stateB);
      push(1, subtrees[2], stateC);
      push(0, subtrees[3], stateD);
      push(1, subtrees[4], stateD);

      AssertThat(ts_stack_merge(stack, 0, 1), IsFalse());
      AssertThat(ts_stack_version_count(stack), Equals<size_t>(2));
    });

    describe("when the merged versions have more than one common entry", [&]() {
      it("combines all of the top common entries", [&]() {
        // . <──0── A <──1── B <──3── D <──5── E*
        //          ↑
        //          └───2─── C <──4── D <──5── E*
        push(0, subtrees[1], stateB);
        push(1, subtrees[2], stateC);
        push(0, subtrees[3], stateD);
        push(1, subtrees[4], stateD);
        push(0, subtrees[5], stateE);
        push(1, subtrees[5], stateE);

        // . <──0── A <──1── B <──3── D <──5── E*
        //          ↑                 |
        //          └───2─── C <──4───┘
        AssertThat(ts_stack_merge(stack, 0, 1), IsTrue());
        AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
        AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
          {stateE, 0},
          {stateD, 1},
          {stateB, 2},
          {stateC, 2},
          {stateA, 3},
          {1, 4},
        })));
      });
    });

    describe("when one of the versions contains an extra (e.g. ERROR) tree of size zero", [&]() {
      it("does not create a loop in the stack", [&]() {
        // . <──0── A <────1──── B*
        //          ↑
        //          └2─ A <──1── B*
        mutate(subtrees[2])->extra = true;
        mutate(subtrees[2])->size = tree_len * 0;

        push(0, subtrees[1], stateB);
        push(1, subtrees[2], stateA);
        push(1, subtrees[1], stateB);

        // . <──0── A <──1── B*
        AssertThat(ts_stack_merge(stack, 0, 1), IsTrue());
        AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
        AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
          {stateB, 0},
          {stateA, 1},
          {1, 2},
        })));
      });
    });
  });

  describe("pop_count(version, count)", [&]() {
    before_each([&]() {
      // . <──0── A <──1── B <──2── C*
      push(0, subtrees[0], stateA);
      push(0, subtrees[1], stateB);
      push(0, subtrees[2], stateC);
    });

    it("creates a new version with the given number of entries removed", [&]() {
      // . <──0── A <──1── B <──2── C*
      //          ↑
      //          └─*
      StackSliceArray pop = ts_stack_pop_count(stack, 0, 2);
      AssertThat(pop.size, Equals<size_t>(1));
      AssertThat(ts_stack_version_count(stack), Equals<size_t>(2));

      StackSlice slice = pop.contents[0];
      AssertThat(slice.version, Equals<StackVersion>(1));
      AssertThat(slice.subtrees, Equals(vector<Subtree>({ subtrees[1], subtrees[2] })));
      AssertThat(ts_stack_state(stack, 1), Equals(stateA));

      free_slice_array(&pool,&pop);
    });

    it("does not count 'extra' subtrees toward the given count", [&]() {
      mutate(subtrees[1])->extra = true;

      // . <──0── A <──1── B <──2── C*
      // ↑
      // └─*
      StackSliceArray pop = ts_stack_pop_count(stack, 0, 2);
      AssertThat(pop.size, Equals<size_t>(1));

      StackSlice slice = pop.contents[0];
      AssertThat(slice.subtrees, Equals(vector<Subtree>({ subtrees[0], subtrees[1], subtrees[2] })));
      AssertThat(ts_stack_state(stack, 1), Equals(1));

      free_slice_array(&pool,&pop);
    });

    describe("when the version has been merged", [&]() {
      before_each([&]() {
        // . <──0── A <──1── B <──2── C <──3── D <──10── I*
        //          ↑                          |
        //          └───4─── E <──5── F <──6───┘
        push(0, subtrees[3], stateD);
        StackSliceArray pop = ts_stack_pop_count(stack, 0, 3);
        free_slice_array(&pool,&pop);
        push(1, subtrees[4], stateE);
        push(1, subtrees[5], stateF);
        push(1, subtrees[6], stateD);
        ts_stack_merge(stack, 0, 1);
        push(0, subtrees[10], stateI);

        AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
        AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
          {stateI, 0},
          {stateD, 1},
          {stateC, 2},
          {stateF, 2},
          {stateB, 3},
          {stateE, 3},
          {stateA, 4},
          {1, 5},
        })));
      });

      describe("when there are two paths that reveal different versions", [&]() {
        it("returns an entry for each revealed version", [&]() {
          // . <──0── A <──1── B <──2── C <──3── D <──10── I*
          //          ↑        ↑
          //          |        └*
          //          |
          //          └───4─── E*
          StackSliceArray pop = ts_stack_pop_count(stack, 0, 3);
          AssertThat(pop.size, Equals<size_t>(2));

          StackSlice slice1 = pop.contents[0];
          AssertThat(slice1.version, Equals<StackVersion>(1));
          AssertThat(slice1.subtrees, Equals(vector<Subtree>({ subtrees[2], subtrees[3], subtrees[10] })));

          StackSlice slice2 = pop.contents[1];
          AssertThat(slice2.version, Equals<StackVersion>(2));
          AssertThat(slice2.subtrees, Equals(vector<Subtree>({ subtrees[5], subtrees[6], subtrees[10] })));

          AssertThat(ts_stack_version_count(stack), Equals<size_t>(3));
          AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
            {stateI, 0},
            {stateD, 1},
            {stateC, 2},
            {stateF, 2},
            {stateB, 3},
            {stateE, 3},
            {stateA, 4},
            {1, 5},
          })));
          AssertThat(get_stack_entries(stack, 1), Equals(vector<StackEntry>({
            {stateB, 0},
            {stateA, 1},
            {1, 2},
          })));
          AssertThat(get_stack_entries(stack, 2), Equals(vector<StackEntry>({
            {stateE, 0},
            {stateA, 1},
            {1, 2},
          })));

          free_slice_array(&pool,&pop);
        });
      });

      describe("when there is one path that ends at a merged version", [&]() {
        it("returns a single entry", [&]() {
          // . <──0── A <──1── B <──2── C <──3── D <──10── I*
          //          |                          |
          //          └───5─── F <──6── G <──7───┘
          //                                     |
          //                                     └*
          StackSliceArray pop = ts_stack_pop_count(stack, 0, 1);
          AssertThat(pop.size, Equals<size_t>(1));

          StackSlice slice1 = pop.contents[0];
          AssertThat(slice1.version, Equals<StackVersion>(1));
          AssertThat(slice1.subtrees, Equals(vector<Subtree>({ subtrees[10] })));

          AssertThat(ts_stack_version_count(stack), Equals<size_t>(2));
          AssertThat(ts_stack_state(stack, 0), Equals(stateI));
          AssertThat(ts_stack_state(stack, 1), Equals(stateD));

          free_slice_array(&pool,&pop);
        });
      });

      describe("when there are two paths that converge on one version", [&]() {
        it("returns two slices with the same version", [&]() {
          // . <──0── A <──1── B <──2── C <──3── D <──10── I*
          //          ↑                          |
          //          ├───4─── E <──5── F <──6───┘
          //          |
          //          └*
          StackSliceArray pop = ts_stack_pop_count(stack, 0, 4);
          AssertThat(pop.size, Equals<size_t>(2));

          StackSlice slice1 = pop.contents[0];
          AssertThat(slice1.version, Equals<StackVersion>(1));
          AssertThat(slice1.subtrees, Equals(vector<Subtree>({ subtrees[1], subtrees[2], subtrees[3], subtrees[10] })));

          StackSlice slice2 = pop.contents[1];
          AssertThat(slice2.version, Equals<StackVersion>(1));
          AssertThat(slice2.subtrees, Equals(vector<Subtree>({ subtrees[4], subtrees[5], subtrees[6], subtrees[10] })));

          AssertThat(ts_stack_version_count(stack), Equals<size_t>(2));
          AssertThat(ts_stack_state(stack, 0), Equals(stateI));
          AssertThat(ts_stack_state(stack, 1), Equals(stateA));

          free_slice_array(&pool,&pop);
        });
      });

      describe("when there are three paths that lead to three different versions", [&]() {
        it("returns three entries with different arrays of subtrees", [&]() {
          // . <──0── A <──1── B <──2── C <──3── D <──10── I*
          //          ↑                          |
          //          ├───4─── E <──5── F <──6───┘
          //          |                          |
          //          └───7─── G <──8── H <──9───┘
          StackSliceArray pop = ts_stack_pop_count(stack, 0, 4);
          free_slice_array(&pool,&pop);
          push(1, subtrees[7], stateG);
          push(1, subtrees[8], stateH);
          push(1, subtrees[9], stateD);
          push(1, subtrees[10], stateI);
          ts_stack_merge(stack, 0, 1);

          AssertThat(ts_stack_version_count(stack), Equals<size_t>(1));
          AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
            {stateI, 0},
            {stateD, 1},
            {stateC, 2},
            {stateF, 2},
            {stateH, 2},
            {stateB, 3},
            {stateE, 3},
            {stateG, 3},
            {stateA, 4},
            {1, 5},
          })));

          // . <──0── A <──1── B <──2── C <──3── D <──10── I*
          //          ↑                 ↑
          //          |                 └*
          //          |
          //          ├───4─── E <──5── F*
          //          |
          //          └───7─── G <──8── H*
          pop = ts_stack_pop_count(stack, 0, 2);
          AssertThat(pop.size, Equals<size_t>(3));

          StackSlice slice1 = pop.contents[0];
          AssertThat(slice1.version, Equals<StackVersion>(1));
          AssertThat(slice1.subtrees, Equals(vector<Subtree>({ subtrees[3], subtrees[10] })));

          StackSlice slice2 = pop.contents[1];
          AssertThat(slice2.version, Equals<StackVersion>(2));
          AssertThat(slice2.subtrees, Equals(vector<Subtree>({ subtrees[6], subtrees[10] })));

          StackSlice slice3 = pop.contents[2];
          AssertThat(slice3.version, Equals<StackVersion>(3));
          AssertThat(slice3.subtrees, Equals(vector<Subtree>({ subtrees[9], subtrees[10] })));

          AssertThat(ts_stack_version_count(stack), Equals<size_t>(4));
          AssertThat(ts_stack_state(stack, 0), Equals(stateI));
          AssertThat(ts_stack_state(stack, 1), Equals(stateC));
          AssertThat(ts_stack_state(stack, 2), Equals(stateF));
          AssertThat(ts_stack_state(stack, 3), Equals(stateH));

          free_slice_array(&pool,&pop);
        });
      });
    });
  });

  describe("pop_pending(version)", [&]() {
    before_each([&]() {
      push(0, subtrees[0], stateA);
    });

    it("removes the top node from the stack if it was pushed in pending mode", [&]() {
      ts_stack_push(stack, 0, subtrees[1], true, stateB);
      ts_subtree_retain(subtrees[1]);

      StackSliceArray pop = ts_stack_pop_pending(stack, 0);
      AssertThat(pop.size, Equals<size_t>(1));

      AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
        {stateA, 0},
        {1, 1},
      })));

      free_slice_array(&pool,&pop);
    });

    it("skips entries whose subtrees are extra", [&]() {
      ts_stack_push(stack, 0, subtrees[1], true, stateB);
      ts_subtree_retain(subtrees[1]);

      mutate(subtrees[2])->extra = true;
      mutate(subtrees[3])->extra = true;

      push(0, subtrees[2], stateB);
      push(0, subtrees[3], stateB);

      StackSliceArray pop = ts_stack_pop_pending(stack, 0);
      AssertThat(pop.size, Equals<size_t>(1));

      AssertThat(pop.contents[0].subtrees, Equals(vector<Subtree>({ subtrees[1], subtrees[2], subtrees[3] })));

      AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
        {stateA, 0},
        {1, 1},
      })));

      free_slice_array(&pool,&pop);
    });

    it("does nothing if the top node was not pushed in pending mode", [&]() {
      push(0, subtrees[1], stateB);

      StackSliceArray pop = ts_stack_pop_pending(stack, 0);
      AssertThat(pop.size, Equals<size_t>(0));

      AssertThat(get_stack_entries(stack, 0), Equals(vector<StackEntry>({
        {stateB, 0},
        {stateA, 1},
        {1, 2},
      })));

      free_slice_array(&pool,&pop);
    });
  });

  describe("setting external token state", [&]() {
    before_each([&]() {
      mutate(subtrees[1])->has_external_tokens = true;
      mutate(subtrees[2])->has_external_tokens = true;
      ts_external_scanner_state_init(&mutate(subtrees[1])->external_scanner_state, NULL, 0);
      ts_external_scanner_state_init(&mutate(subtrees[2])->external_scanner_state, NULL, 0);
    });

    it("allows the state to be retrieved", [&]() {
      AssertThat(ts_stack_last_external_token(stack, 0).ptr, Equals<const SubtreeHeapData *>(nullptr));

      ts_stack_set_last_external_token(stack, 0, subtrees[1]);
      AssertThat(ts_stack_last_external_token(stack, 0).ptr, Equals<const SubtreeHeapData *>(subtrees[1].ptr));

      ts_stack_copy_version(stack, 0);
      AssertThat(ts_stack_last_external_token(stack, 1).ptr, Equals<const SubtreeHeapData *>(subtrees[1].ptr));

      ts_stack_set_last_external_token(stack, 0, subtrees[2]);
      AssertThat(ts_stack_last_external_token(stack, 0).ptr, Equals<const SubtreeHeapData *>(subtrees[2].ptr));
    });

    it("does not merge stack versions with different external token states", [&]() {
      ts_external_scanner_state_init(&mutate(subtrees[1])->external_scanner_state, "abcd", 2);
      ts_external_scanner_state_init(&mutate(subtrees[2])->external_scanner_state, "ABCD", 2);

      ts_stack_copy_version(stack, 0);
      push(0, subtrees[0], 5);
      push(1, subtrees[0], 5);

      ts_stack_set_last_external_token(stack, 0, subtrees[1]);
      ts_stack_set_last_external_token(stack, 1, subtrees[2]);

      AssertThat(ts_stack_merge(stack, 0, 1), IsFalse());
    });

    it("merges stack versions with identical external token states", [&]() {
      ts_external_scanner_state_init(&mutate(subtrees[1])->external_scanner_state, "abcd", 2);
      ts_external_scanner_state_init(&mutate(subtrees[2])->external_scanner_state, "abcd", 2);

      ts_stack_copy_version(stack, 0);
      push(0, subtrees[0], 5);
      push(1, subtrees[0], 5);

      ts_stack_set_last_external_token(stack, 0, subtrees[1]);
      ts_stack_set_last_external_token(stack, 1, subtrees[2]);

      AssertThat(ts_stack_merge(stack, 0, 1), IsTrue());
    });

    it("does not distinguish between an *empty* external token state and *no* external token state", [&]() {
      ts_stack_copy_version(stack, 0);
      push(0, subtrees[0], 5);
      push(1, subtrees[0], 5);

      ts_stack_set_last_external_token(stack, 0, subtrees[1]);

      AssertThat(ts_stack_merge(stack, 0, 1), IsTrue());
    });
  });
});

END_TEST

bool operator==(const StackEntry &left, const StackEntry &right) {
  return left.state == right.state && left.depth == right.depth;
}

std::ostream &operator<<(std::ostream &stream, const StackEntry &entry) {
  return stream << "{" << entry.state << ", " << entry.depth << "}";
}

std::ostream &operator<<(std::ostream &stream, const SubtreeArray &array) {
  stream << "[";
  bool first = true;
  for (size_t i = 0; i < array.size; i++) {
    if (!first)
      stream << ", ";
    first = false;
    stream << array.contents[i];
  }
  return stream << "]";
}
