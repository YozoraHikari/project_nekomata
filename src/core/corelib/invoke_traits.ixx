export module projnekomata.corelib:invoke_traits;
import std;

export template <typename Fn, typename ReturnType, typename... Args> concept TypedInvocable = std::invocable<Fn, Args...> && std::same_as<std::invoke_result_t<Fn, Args...>, ReturnType>;
export template <typename Fn, typename... Args> concept TypedInvocableNoRet = std::invocable<Fn, Args...>;
