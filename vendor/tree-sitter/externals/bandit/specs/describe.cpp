#include <specs/fakes/fake_context.h>
#include <specs/fakes/fake_reporter.h>
#include <specs/specs.h>

using namespace bandit::fakes;

SPEC_BEGIN(describe)

describe("describe", []() {
  std::function<void()> describe_fn;
  fake_reporter* reporter;
  std::unique_ptr<bandit::detail::controller_t> controller;
  std::unique_ptr<fake_context> global_context;

  before_each([&]() {
    reporter = new fake_reporter();
    controller.reset(new bandit::detail::controller_t());
    controller->set_reporter(reporter);

    global_context.reset(new fake_context());
    controller->get_contexts().push_back(global_context.get());
  });

  auto call_describe = [&]() {
    describe("context name", describe_fn, false, *controller);
  };

  describe("with a succeeding 'it'", [&]() {
    int context_stack_size_while_running;

    before_each([&]() {
      context_stack_size_while_running = 0;
      describe_fn = [&]() { context_stack_size_while_running = controller->get_contexts().size(); };
    });

    it("tells its parent context that execution has started", [&]() {
      // This is important as once execution has started,
      // before_each and after_each calls cannot be guaranteed to
      // be run before any 'it' method.

      call_describe();
      AssertThat(global_context->call_log(), Has().AtLeast(1).EqualTo("execution_is_starting"));
    });

    it("tells reporter it's starting a run", [&]() {
      call_describe();
      AssertThat(reporter->call_log(), Has().Exactly(1).EqualTo("context_starting: context name"));
    });

    it("tells reporter it's finished a run", [&]() {
      call_describe();
      AssertThat(reporter->call_log(), Has().Exactly(1).EqualTo("context_ended: context name"));
    });

    it("pushes a new context during execution", [&]() {
      call_describe();
      AssertThat(context_stack_size_while_running, Equals(2));
    });

    it("pops the context from the stack after execution so that only the global context is left", [&]() {
      call_describe();
      AssertThat(controller->get_contexts(), Is().OfLength(1));
    });

  });

  describe("with test run error", [&]() {
    // This can occur if after_each or before_each are called
    // after execution has started for a context.

    before_each([&]() {
      describe_fn = [&]() { throw bandit::detail::test_run_error("we dun goofed!"); };
    });

    it("doesn't propagate the error", [&]() {
      call_describe();
    });

    it("tells reporter to report the error", [&]() {
      call_describe();
      AssertThat(reporter->call_log(), Has().Exactly(1).EqualTo("test_run_error: context name (we dun goofed!)"));
    });

  });

  describe("skip", [&]() {
    bool context_is_hard_skip;
    describe_fn = [&]() {
      context_is_hard_skip = controller->get_contexts().back()->hard_skip();
    };

    before_each([&]() {
      context_is_hard_skip = false;
    });

    describe("describe_skip", [&]() {
      it("pushes a context marked as skipped on the stack", [&]() {
        describe_skip("context name", describe_fn, *controller);
        AssertThat(context_is_hard_skip, IsTrue());
      });
    });

    describe("xdescribe", [&]() {
      it("pushes a context marked as skipped on the stack", [&]() {
        xdescribe("context name", describe_fn, *controller);
        AssertThat(context_is_hard_skip, IsTrue());
      });
    });
  });
});

SPEC_END
