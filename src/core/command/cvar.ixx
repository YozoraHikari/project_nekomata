export module projnekomata:core.command.cvar;
import std;
import projnekomata.corelib;

export namespace projnekomata {

class ICvar {
public:
    virtual ~ICvar() = default;
    virtual auto getName() const -> const std::string& = 0;

    virtual auto setFromString(std::string_view value) -> void = 0;
    virtual auto toString() const -> std::string = 0;
};

template <typename T> class Cvar : public ICvar {
public:
    Cvar(std::string name, T defaultValue) : m_name(name), m_value(defaultValue) {}

    T get() { return m_value.load(std::memory_order_acquire); }

    void set(T value) {
        m_value.store(value, std::memory_order_release);
        if (m_onChangeCallback)
            m_onChangeCallback(*this);
    }

    void onChangeCallback(std::function<void(const Cvar<T>&)> callback) {
        m_onChangeCallback = std::move(callback);
    }

    auto getName() const -> const std::string& override { return m_name; }
    auto setFromString(std::string_view value) -> void override {
        if constexpr (std::is_same_v<T, bool>) {
            if (value == "true")
                set(true);
            else if (value == "false")
                set(false);
            else
                log::error("invalid value for cvar '{}': {}", m_name, value);
            return;
        }
        std::istringstream iss(value);
        T tmp;
        iss >> tmp;
        if (iss.fail()) {
            log::error("invalid value for cvar '{}': {}", m_name, value);
            return;
        }
        set(tmp);
    }
    auto toString() const -> std::string override { return std::to_string(m_value); }

private:
    std::string m_name;
    std::atomic<T> m_value;
    std::function<void(const Cvar<T>&)> m_onChangeCallback = nullptr;
};

template<> class Cvar<std::string> : ICvar {
public:
    Cvar(std::string name, std::string defaultValue) : m_name(std::move(name)), m_value(std::move(defaultValue)) {}

    const std::string& get() {
        std::shared_lock lock(m_mutex);
        return m_value;
    }

    void set(std::string value) {
        std::unique_lock lock(m_mutex);
        m_value = std::move(value);
        if (m_onChangeCallback)
            m_onChangeCallback(*this);
    }

    void onChangeCallback(std::function<void(const Cvar<std::string>&)> callback) {
        m_onChangeCallback = std::move(callback);
    }

    auto getName() const -> const std::string& override { return m_name; }
    auto setFromString(std::string_view value) -> void override { set(std::string(value)); }
    auto toString() const -> std::string override { return m_value; }

private:
    std::string m_name;
    std::string m_value;
    std::shared_mutex m_mutex;
    std::function<void(const Cvar<std::string>&)> m_onChangeCallback = nullptr;
};

class CvarManager {
public:
    static auto get() -> CvarManager& {
        static CvarManager instance;
        return instance;
    }

    template <typename T> auto registerCvar(std::string name, T defaultValue) -> Cvar<T>& {
        std::unique_lock lock(m_mutex);
        if (m_cvars.contains(name)) panic("cvar with name '{}' already exists", name);

        auto cvar = Unique<Cvar<T>>::create(name, std::move(defaultValue));
        auto icvar = Unique<ICvar>::upcast(std::move(cvar));
        auto& cvarref = m_cvars.insert(std::move(name), std::move(icvar));

        return *static_cast<Cvar<T>*>(cvarref.ptr());
    }

    auto getCvar(const std::string& name) -> ICvar* {
        std::shared_lock lock(m_mutex);

        if (!m_cvars.contains(name)) return nullptr;

        return m_cvars.get(name).unwrap().get().ptr();
    }

private:
    CvarManager() = default;

    std::shared_mutex m_mutex;
    HashMap<std::string, Unique<ICvar>> m_cvars = HashMap<std::string, Unique<ICvar>>::create();
};

}