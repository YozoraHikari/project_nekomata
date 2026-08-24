export module projnekomata:core.containers.mpsc_queue;
import std;
import projnekomata.corelib;

export template <typename T> class AtomicMpscQueue {
public:
    AtomicMpscQueue() {
        Node* dummy = new Node();
        m_head.store(dummy);
        m_tail = dummy;
    }


    ~AtomicMpscQueue() {
        while (pop().isSome())
            ;
        delete m_tail;
    }

    auto push(T value) -> void {
        Node* node = new Node(std::move(value));
        Node* prev = m_head.exchange(node, std::memory_order_acq_rel);
        prev->m_next.store(node, std::memory_order_release);
    }

    auto pop() -> Option<T> {
        Node* next = m_tail->m_next.load(std::memory_order_acquire);
        if (!next)
            return None;

        auto result = std::move(next->m_value);

        Node* oldTail = m_tail;
        m_tail = next;
        delete oldTail;
        return result;
    }

    auto peek() -> Option<std::reference_wrapper<T>> {
        Node* next = m_tail->m_next.load(std::memory_order_acquire);
        if (!next)
            return None;

        if (!next->m_value.has_value())
            return None;

        return Some(std::ref(next->m_value.value()));
    }

    auto hasElement() -> bool {
        Node* next = m_tail->m_next.load(std::memory_order_acquire);
        return next != nullptr;
    }

private:
    struct Node {
        Option<T> m_value = None;
        std::atomic<Node*> m_next{nullptr};

        Node() = default;
        Node(T v) : m_value(Some(std::move(v))) {}
    };

    std::atomic<Node*> m_head;
    Node* m_tail;
};