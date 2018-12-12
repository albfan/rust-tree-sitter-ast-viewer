#include "test_helper.h"
#include "compiler/prepare_grammar/flatten_grammar.h"
#include "compiler/prepare_grammar/initial_syntax_grammar.h"
#include "compiler/syntax_grammar.h"
#include "helpers/stream_methods.h"

START_TEST

using namespace rules;
using prepare_grammar::flatten_rule;

describe("flatten_grammar", []() {
  it("associates each symbol with the precedence and associativity binding it to its successor", [&]() {
    SyntaxVariable result = flatten_rule({
      "test",
      VariableTypeNamed,
      Rule::seq({
        Symbol::non_terminal(1),
        Metadata::prec_left(101, Rule::seq({
          Symbol::non_terminal(2),
          Rule::choice({
            Metadata::prec_right(102, Rule::seq({
              Symbol::non_terminal(3),
              Symbol::non_terminal(4)
            })),
            Symbol::non_terminal(5),
          }),
          Symbol::non_terminal(6),
        })),
        Symbol::non_terminal(7),
      })
    });

    AssertThat(result.name, Equals("test"));
    AssertThat(result.type, Equals(VariableTypeNamed));
    AssertThat(result.productions, Equals(vector<Production>({
      Production({
        {Symbol::non_terminal(1), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(2), 101, AssociativityLeft, Alias{}},
        {Symbol::non_terminal(3), 102, AssociativityRight, Alias{}},
        {Symbol::non_terminal(4), 101, AssociativityLeft, Alias{}},
        {Symbol::non_terminal(6), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(7), 0, AssociativityNone, Alias{}},
      }, 0),
      Production({
        {Symbol::non_terminal(1), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(2), 101, AssociativityLeft, Alias{}},
        {Symbol::non_terminal(5), 101, AssociativityLeft, Alias{}},
        {Symbol::non_terminal(6), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(7), 0, AssociativityNone, Alias{}},
      }, 0)
    })));
  });

  it("stores the maximum dynamic precedence specified in each production", [&]() {
    SyntaxVariable result = flatten_rule({
      "test",
      VariableTypeNamed,
      Rule::seq({
        Symbol::non_terminal(1),
        Metadata::prec_dynamic(101, Rule::seq({
          Symbol::non_terminal(2),
          Rule::choice({
            Metadata::prec_dynamic(102, Rule::seq({
              Symbol::non_terminal(3),
              Symbol::non_terminal(4)
            })),
            Symbol::non_terminal(5),
          }),
          Symbol::non_terminal(6),
        })),
        Symbol::non_terminal(7),
      })
    });

    AssertThat(result.name, Equals("test"));
    AssertThat(result.type, Equals(VariableTypeNamed));
    AssertThat(result.productions, Equals(vector<Production>({
      Production({
        {Symbol::non_terminal(1), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(2), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(3), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(4), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(6), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(7), 0, AssociativityNone, Alias{}},
      }, 102),
      Production({
        {Symbol::non_terminal(1), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(2), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(5), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(6), 0, AssociativityNone, Alias{}},
        {Symbol::non_terminal(7), 0, AssociativityNone, Alias{}},
      }, 101),
    })));
  });

  it("uses the last assigned precedence", [&]() {
    SyntaxVariable result = flatten_rule({
      "test1",
      VariableTypeNamed,
      Metadata::prec_left(101, Rule::seq({
        Symbol::non_terminal(1),
        Symbol::non_terminal(2),
      }))
    });

    AssertThat(result.productions, Equals(vector<Production>({
      Production({
        {Symbol::non_terminal(1), 101, AssociativityLeft, Alias{}},
        {Symbol::non_terminal(2), 101,  AssociativityLeft, Alias{}},
      }, 0)
    })));

    result = flatten_rule({
      "test2",
      VariableTypeNamed,
      Metadata::prec_left(101, Rule::seq({
        Symbol::non_terminal(1),
      }))
    });

    AssertThat(result.productions, Equals(vector<Production>({
      Production({
        {Symbol::non_terminal(1), 101, AssociativityLeft, Alias{}},
      }, 0)
    })));
  });
});

END_TEST
