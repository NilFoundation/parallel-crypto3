//---------------------------------------------------------------------------//
// Copyright (c) 2011-2018 Dominik Charousset
// Copyright (c) 2017-2020 Mikhail Komarov <nemo@nil.foundation>
//
// Distributed under the terms and conditions of the BSD 3-Clause License or
// (at your option) under the terms and conditions of the Boost Software
// License 1.0. See accompanying files LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt.
//---------------------------------------------------------------------------//

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>

#include <nil/actor/abstract_actor.hpp>
#include <nil/actor/actor_cast.hpp>
#include <nil/actor/actor_clock.hpp>
#include <nil/actor/actor_config.hpp>
#include <nil/actor/actor_profiler.hpp>
#include <nil/actor/actor_registry.hpp>
#include <nil/actor/actor_traits.hpp>
#include <nil/actor/composable_behavior_based_actor.hpp>

#include <nil/actor/detail/init_fun_factory.hpp>
#include <nil/actor/detail/spawn_fwd.hpp>
#include <nil/actor/detail/spawnable.hpp>
#include <nil/actor/fwd.hpp>
#include <nil/actor/group_manager.hpp>
#include <nil/actor/infer_handle.hpp>
#include <nil/actor/is_typed_actor.hpp>
#include <nil/actor/logger.hpp>
#include <nil/actor/make_actor.hpp>
#include <nil/actor/prohibit_top_level_spawn_marker.hpp>
#include <nil/actor/scoped_execution_unit.hpp>
#include <nil/actor/spawn_options.hpp>
#include <nil/actor/string_algorithms.hpp>
#include <nil/actor/type_id.hpp>

#include <nil/module/nameable.hpp>
#include <nil/module/identifiable.hpp>
#include <nil/module/configurable.hpp>
#include <nil/module/initializable.hpp>

namespace nil {
    namespace actor {
        namespace detail {

            template<class>
            struct typed_mpi_access;

            template<class... In, class... Out>
            struct typed_mpi_access<typed_mpi<type_list<In...>, output_tuple<Out...>>> {
                std::string operator()() const {
                    static_assert(sizeof...(In) > 0, "typed MPI without inputs");
                    static_assert(sizeof...(Out) > 0, "typed MPI without outputs");
                    std::vector<std::string> inputs {type_name_v<In>...};
                    std::vector<std::string> outputs1 {type_name_v<Out>...};
                    std::string result = "nil::actor::replies_to<";
                    result += join(inputs, ",");
                    result += ">::with<";
                    result += join(outputs1, ",");
                    result += ">";
                    return result;
                }
            };

            template<class T>
            std::string get_rtti_from_mpi() {
                typed_mpi_access<T> f;
                return f();
            }

        }    // namespace detail
    }        // namespace actor
}    // namespace nil

namespace nil {
    namespace actor {

        template<typename ConfigurationType, typename OptionsType>
        struct BOOST_SYMBOL_VISIBLE spawner_module : public module::identifiable<uint32_t>,
                                                     public module::nameable<const char *>,
                                                     public module::configurable<ConfigurationType, OptionsType>,
                                                     public module::initializable {
            typedef ConfigurationType configuration_type;
            typedef OptionsType options_type;

            virtual ~spawner_module() {
            }

            /// Returns the human-redable name of the module.
            virtual const char *name() const override = 0;

            /// Starts any background threads needed by the module.
            virtual void startup() override = 0;

            /// Stops all background threads of the module.
            virtual void shutdown() override = 0;

            /// Allows the module to change the
            /// configuration of the actor system during startup.
            virtual void initialize(ConfigurationType &cfg) override = 0;

            virtual void set_options(ConfigurationType &cfg, ConfigurationType &c) override = 0;

            /// Returns the identifier of this module.
            virtual id_t id() const = 0;

            /// Returns a pointer to the subtype.
            virtual void *subtype_ptr() = 0;
        };

        /// An (optional) component of the actor system with networking capabilities.
        template<typename ConfigurationType, typename OptionsType>
        class BOOST_SYMBOL_VISIBLE networking_module : public spawner_module<ConfigurationType, OptionsType> {
        public:
            virtual ~networking_module() override {
            }

            /// Causes the module to send a `node_down_msg` to `observer` if this system
            /// loses connection to `node`.
            virtual void monitor(const node_id &node, const actor_addr &observer) = 0;

            /// Causes the module remove one entry for `observer` from the list of
            /// actors that receive a `node_down_msg` if this system loses connection to
            /// `node`. Each call to `monitor` requires one call to `demonitor` in order
            /// to unsubscribe the `observer` completely.
            virtual void demonitor(const node_id &node, const actor_addr &observer) = 0;
        };

        /// Actor environment including scheduler, registry, and optional components
        /// such as a middleman.
        class BOOST_SYMBOL_VISIBLE spawner {
        public:
            friend class logger;
            friend class io::middleman;
            friend class network::middleman;
            friend class abstract_actor;

            /// Returns the internal actor for dynamic spawn operations.
            const strong_actor_ptr &spawn_serv() const {
                return spawn_serv_;
            }

            /// Returns the internal actor for storing the runtime configuration
            /// for this actor system.
            const strong_actor_ptr &config_serv() const {
                return config_serv_;
            }

            spawner() = delete;
            spawner(const spawner &) = delete;
            spawner &operator=(const spawner &) = delete;

            using module_ptr = std::unique_ptr<spawner_module>;

            using module_array = std::array<module_ptr, spawner_module::num_ids>;

            /// @warning The system stores a reference to `cfg`, which means the
            ///          config object must outlive the actor system.
            explicit spawner(spawner_config &cfg);

            virtual ~spawner();

            /// A message passing interface (MPI) in run-time checkable representation.
            using mpi = std::set<std::string>;

            template<class T, class E = typename std::enable_if<!is_typed_actor<T>::value>::type>
            mpi message_types(detail::type_list<T>) const {
                return mpi {};
            }

            template<class... Ts>
            mpi message_types(detail::type_list<typed_actor<Ts...>>) const {
                static_assert(sizeof...(Ts) > 0, "empty typed actor handle given");
                mpi result {detail::get_rtti_from_mpi<Ts>()...};
                return result;
            }

            template<class T, class E = typename std::enable_if<!detail::is_type_list<T>::value>::type>
            mpi message_types(const T &) const {
                detail::type_list<T> token;
                return message_types(token);
            }

            /// Returns a string representation of the messaging
            /// interface using portable names;
            template<class T>
            mpi message_types() const {
                detail::type_list<T> token;
                return message_types(token);
            }

            /// Returns whether actor handles described by `xs`
            /// can be assigned to actor handles described by `ys`.
            /// @experimental
            bool assignable(const mpi &xs, const mpi &ys) const {
                if (ys.empty())
                    return xs.empty();
                if (xs.size() == ys.size())
                    return xs == ys;
                return std::includes(xs.begin(), xs.end(), ys.begin(), ys.end());
            }

            /// Returns whether actor handles described by `xs`
            /// can be assigned to actor handles of type `T`.
            /// @experimental
            template<class T>
            bool assignable(const std::set<std::string> &xs) const {
                return assignable(xs, message_types<T>());
            }

            /// Returns the host-local identifier for this system.
            const node_id &node() const;

            /// Returns the scheduler instance.
            scheduler::abstract_coordinator &scheduler();

            /// Returns the system-wide event logger.
            nil::actor::logger &logger();

            /// Returns the system-wide actor registry.
            actor_registry &registry();

            /// Returns a string representation for `err`.
            std::string render(const error &x) const;

            /// Returns the system-wide group manager.
            group_manager &groups();

            /// Returns a dummy execution unit that forwards
            /// everything to the scheduler.
            scoped_execution_unit *dummy_execution_unit();

            /// Returns a new actor ID.
            actor_id next_actor_id();

            /// Returns the last given actor ID.
            actor_id latest_actor_id() const;

            /// Blocks this caller until all actors are done.
            void await_all_actors_done() const;

            /// Send a `node_down_msg` to `observer` if this system loses connection to
            /// `node`.
            /// @note Calling this function *n* times causes the system to send
            ///       `node_down_msg` *n* times to the observer. In order to not receive
            ///       the messages, the observer must call `demonitor` *n* times.
            void monitor(const node_id &node, const actor_addr &observer);

            /// Removes `observer` from the list of actors that receive a `node_down_msg`
            /// if this system loses connection to `node`.
            void demonitor(const node_id &node, const actor_addr &observer);

            /// Called by `spawn` when used to create a class-based actor to
            /// apply automatic conversions to `xs` before spawning the actor.
            /// Should not be called by users of the library directly.
            /// @param cfg To-be-filled config for the actor.
            /// @param xs Constructor arguments for `C`.
            template<class C, spawn_options Os, class... Ts>
            infer_handle_from_class_t<C> spawn_class(actor_config &cfg, Ts &&... xs) {
                return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
            }

            /// Returns a new actor of type `C` using `xs...` as constructor
            /// arguments. The behavior of `spawn` can be modified by setting `Os`, e.g.,
            /// to opt-out of the cooperative scheduling.
            /// @param xs Constructor arguments for `C`.
            template<class C, spawn_options Os = no_spawn_options, class... Ts>
            infer_handle_from_class_t<C> spawn(Ts &&... xs) {
                check_invariants<C>();
                actor_config cfg;
                return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
            }

            template<class S, spawn_options Os = no_spawn_options>
            infer_handle_from_state_t<S> spawn() {
                return spawn<composable_behavior_based_actor<S>, Os>();
            }

            /// Called by `spawn` when used to create a functor-based actor to select a
            /// proper implementation and then delegates to `spawn_impl`.
            /// @param cfg To-be-filled config for the actor.
            /// @param fun Function object for the actor's behavior; will be moved.
            /// @param xs Arguments for `fun`.
            /// @private
            template<spawn_options Os = no_spawn_options, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_functor(std::true_type, actor_config &cfg, F &fun, Ts &&... xs) {
                using impl = infer_impl_from_fun_t<F>;
                detail::init_fun_factory<impl, F> fac;
                cfg.init_fun = fac(std::move(fun), std::forward<Ts>(xs)...);
                return spawn_impl<impl, Os>(cfg);
            }

            /// Fallback no-op overload.
            /// @private
            template<spawn_options Os = no_spawn_options, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_functor(std::false_type, actor_config &, F &, Ts &&...) {
                return {};
            }

            /// Returns a new functor-based actor. The first argument must be the functor,
            /// the remainder of `xs...` is used to invoke the functor.
            /// The behavior of `spawn` can be modified by setting `Os`, e.g.,
            /// to opt-out of the cooperative scheduling.
            template<spawn_options Os = no_spawn_options, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn(F fun, Ts &&... xs) {
                using impl = infer_impl_from_fun_t<F>;
                check_invariants<impl>();
                static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
                static_assert(spawnable, "cannot spawn function-based actor with given arguments");
                actor_config cfg;
                return spawn_functor<Os>(detail::bool_token<spawnable> {}, cfg, fun, std::forward<Ts>(xs)...);
            }

            /// Returns a new actor with run-time type `name`, constructed
            /// with the arguments stored in `args`.
            /// @experimental
            template<class Handle, class E = typename std::enable_if<is_handle<Handle>::value>::type>
            expected<Handle> spawn(const std::string &name, message args, execution_unit *ctx = nullptr,
                                   bool check_interface = true, const mpi *expected_ifs = nullptr) {
                mpi tmp;
                if (check_interface && !expected_ifs) {
                    tmp = message_types<Handle>();
                    expected_ifs = &tmp;
                }
                auto res = dyn_spawn_impl(name, args, ctx, check_interface, expected_ifs);
                if (!res)
                    return std::move(res.error());
                return actor_cast<Handle>(std::move(*res));
            }

            /// Spawns a class-based actor `T` immediately joining the groups in
            /// range `[first, last)`.
            /// @private
            template<class T, spawn_options Os, class Iter, class... Ts>
            infer_handle_from_class_t<T> spawn_class_in_groups(actor_config &cfg, Iter first, Iter last, Ts &&... xs) {
                static_assert(std::is_same<infer_handle_from_class_t<T>, actor>::value,
                              "only dynamically-typed actors can be spawned in a group");
                check_invariants<T>();
                auto irange = make_input_range(first, last);
                cfg.groups = &irange;
                return spawn_class<T, Os>(cfg, std::forward<Ts>(xs)...);
            }

            /// Spawns a class-based actor `T` immediately joining the groups in
            /// range `[first, last)`.
            /// @private
            template<spawn_options Os, class Iter, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_fun_in_groups(actor_config &cfg, Iter first, Iter second, F &fun,
                                                           Ts &&... xs) {
                using impl = infer_impl_from_fun_t<F>;
                check_invariants<impl>();
                using traits = actor_traits<impl>;
                static_assert(traits::is_dynamically_typed, "only dynamically-typed actors can join groups");
                static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
                static_assert(spawnable, "cannot spawn function-based actor with given arguments");
                static constexpr bool enabled = traits::is_dynamically_typed && spawnable;
                auto irange = make_input_range(first, second);
                cfg.groups = &irange;
                return spawn_functor<Os>(detail::bool_token<enabled> {}, cfg, fun, std::forward<Ts>(xs)...);
            }

            /// Returns a new functor-based actor subscribed to all groups in `gs`.
            template<spawn_options Os = no_spawn_options, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_in_groups(std::initializer_list<group> gs, F fun, Ts &&... xs) {
                actor_config cfg;
                return spawn_fun_in_groups<Os>(cfg, gs.begin(), gs.end(), fun, std::forward<Ts>(xs)...);
            }

            /// Returns a new functor-based actor subscribed to all groups in `gs`.
            template<spawn_options Os = no_spawn_options, class Gs, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_in_groups(const Gs &gs, F fun, Ts &&... xs) {
                actor_config cfg;
                return spawn_fun_in_groups<Os>(cfg, gs.begin(), gs.end(), fun, std::forward<Ts>(xs)...);
            }

            /// Returns a new functor-based actor subscribed to all groups in `gs`.
            template<spawn_options Os = no_spawn_options, class F, class... Ts>
            infer_handle_from_fun_t<F> spawn_in_group(const group &grp, F fun, Ts &&... xs) {
                return spawn_in_groups<Os>({grp}, std::move(fun), std::forward<Ts>(xs)...);
            }

            /// Returns a new class-based actor subscribed to all groups in `gs`.
            template<class T, spawn_options Os = no_spawn_options, class... Ts>
            infer_handle_from_class_t<T> spawn_in_groups(std::initializer_list<group> gs, Ts &&... xs) {
                actor_config cfg;
                return spawn_class_in_groups<T, Os>(cfg, gs.begin(), gs.end(), std::forward<Ts>(xs)...);
            }

            /// Returns a new class-based actor subscribed to all groups in `gs`.
            template<class T, spawn_options Os = no_spawn_options, class Gs, class... Ts>
            infer_handle_from_class_t<T> spawn_in_groups(const Gs &gs, Ts &&... xs) {
                actor_config cfg;
                return spawn_class_in_groups<T, Os>(cfg, gs.begin(), gs.end(), std::forward<Ts>(xs)...);
            }

            /// Returns a new class-based actor subscribed to all groups in `gs`.
            template<class T, spawn_options Os = no_spawn_options, class... Ts>
            infer_handle_from_class_t<T> spawn_in_group(const group &grp, Ts &&... xs) {
                return spawn_in_groups<T, Os>({grp}, std::forward<Ts>(xs)...);
            }

            /// Returns whether this actor system calls `await_all_actors_done`
            /// in its destructor before shutting down.
            bool await_actors_before_shutdown() const {
                return await_actors_before_shutdown_;
            }

            /// Configures whether this actor system calls `await_all_actors_done`
            /// in its destructor before shutting down.
            void await_actors_before_shutdown(bool x) {
                await_actors_before_shutdown_ = x;
            }

            /// Returns the configuration of this actor system.
            const spawner_config &config() const {
                return cfg_;
            }

            /// Returns the system-wide clock.
            actor_clock &clock() noexcept;

            /// Returns the number of detached actors.
            size_t detached_actors() {
                return detached_.load();
            }

            /// @cond PRIVATE

            /// Increases running-detached-threads-count by one.
            void inc_detached_threads();

            /// Decreases running-detached-threads-count by one.
            void dec_detached_threads();

            /// Blocks the caller until all detached threads are done.
            void await_detached_threads();

            /// Calls all thread started hooks
            /// @warning must be called by thread which is about to start
            void thread_started();

            /// Calls all thread terminates hooks
            /// @warning must be called by thread which is about to terminate
            void thread_terminates();

            template<class C, spawn_options Os, class... Ts>
            infer_handle_from_class_t<C> spawn_impl(actor_config &cfg, Ts &&... xs) {
                static_assert(is_unbound(Os), "top-level spawns cannot have monitor or link flag");
                // TODO: use `if constexpr` when switching to C++17
                if constexpr (has_detach_flag(Os) || std::is_base_of<blocking_actor, C>::value)
                    cfg.flags |= abstract_actor::is_detached_flag;
                if constexpr (has_hide_flag(Os))
                    cfg.flags |= abstract_actor::is_hidden_flag;
                if (cfg.host == nullptr)
                    cfg.host = dummy_execution_unit();
                ACTOR_SET_LOGGER_SYS(this);
                auto res = make_actor<C>(next_actor_id(), node(), this, cfg, std::forward<Ts>(xs)...);
                auto ptr = static_cast<C *>(actor_cast<abstract_actor *>(res));
#ifdef ACTOR_ENABLE_ACTOR_PROFILER
                profiler_add_actor(*ptr, cfg.parent);
#endif
                ptr->launch(cfg.host, has_lazy_init_flag(Os), has_hide_flag(Os));
                return res;
            }

            void profiler_add_actor(const local_actor &self, const local_actor *parent) {
                if (profiler_ != nullptr)
                    profiler_->add_actor(self, parent);
            }

            void profiler_remove_actor(const local_actor &self) {
                if (profiler_ != nullptr)
                    profiler_->remove_actor(self);
            }

            void profiler_before_processing(const local_actor &self, const mailbox_element &element) {
                if (profiler_ != nullptr)
                    profiler_->before_processing(self, element);
            }

            void profiler_after_processing(const local_actor &self, invoke_message_result result) {
                if (profiler_ != nullptr)
                    profiler_->after_processing(self, result);
            }

            void profiler_before_sending(const local_actor &self, mailbox_element &element) {
                if (profiler_ != nullptr)
                    profiler_->before_sending(self, element);
            }

            void profiler_before_sending_scheduled(const local_actor &self,
                                                   nil::actor::actor_clock::time_point timeout,
                                                   mailbox_element &element) {
                if (profiler_ != nullptr)
                    profiler_->before_sending_scheduled(self, timeout, element);
            }

            tracing_data_factory *tracing_context() const noexcept {
                return tracing_context_;
            }

            /// @endcond

        private:
            template<class T>
            void check_invariants() {
                static_assert(!std::is_base_of<prohibit_top_level_spawn_marker, T>::value,
                              "This actor type cannot be spawned through an actor system. "
                              "Probably you have tried to spawn a broker.");
            }

            expected<strong_actor_ptr> dyn_spawn_impl(const std::string &name, message &args, execution_unit *ctx,
                                                      bool check_interface, optional<const mpi &> expected_ifs);

            /// Sets the internal actor for dynamic spawn operations.
            void spawn_serv(strong_actor_ptr x) {
                spawn_serv_ = std::move(x);
            }

            /// Sets the internal actor for storing the runtime configuration.
            void config_serv(strong_actor_ptr x) {
                config_serv_ = std::move(x);
            }

            // -- member variables -------------------------------------------------------

            /// Provides system-wide callbacks for several actor operations.
            actor_profiler *profiler_;

            /// Used to generate ascending actor IDs.
            std::atomic<size_t> ids_;

            /// Identifies this actor system in a distributed setting.
            node_id node_;

            /// Manages log output.
            intrusive_ptr<nil::actor::logger> logger_;

            /// Maps well-known actor names to actor handles.
            actor_registry registry_;

            /// Maps well-known group names to group handles.
            group_manager groups_;

            /// Stores optional actor system components.
            module_array modules_;

            /// Provides pseudo scheduling context to actors.
            scoped_execution_unit dummy_execution_unit_;

            /// Stores whether the system should wait for running actors on shutdown.
            bool await_actors_before_shutdown_;

            /// Stores config parameters.
            strong_actor_ptr config_serv_;

            /// Allows fully dynamic spawning of actors.
            strong_actor_ptr spawn_serv_;

            /// Counts the number of detached actors.
            std::atomic<size_t> detached_;

            /// Guards `detached`.
            mutable std::mutex detached_mtx_;

            /// Allows waiting on specific values for `detached`.
            mutable std::condition_variable detached_cv_;

            /// The system-wide, user-provided configuration.
            spawner_config &cfg_;

            /// Stores whether the logger has run its destructor and stopped any thread,
            /// file handle, etc.
            std::atomic<bool> logger_dtor_done_;

            /// Guards `logger_dtor_done_`.
            mutable std::mutex logger_dtor_mtx_;

            /// Allows waiting on specific values for `logger_dtor_done_`.
            mutable std::condition_variable logger_dtor_cv_;

            /// Stores the system-wide factory for deserializing tracing data.
            tracing_data_factory *tracing_context_;
        };
    }    // namespace actor
}    // namespace nil
