#ifndef EVENT_H
#define EVENT_H

#include <functional>
#include <algorithm>
#include <utility>
#include <future>
#include <atomic>
#include <list>
#include <mutex>

template <typename... Args>
class GalaxyHandler
{
public:
    typedef std::function<void(Args...)> handler;
    typedef unsigned int handler_id_type;

    void operator()(Args... params) const
    {
        if(m_handlerFunc) m_handlerFunc(params);
    }

    explicit event_handler(const handler& handlerFunc) : m_handlerFunc(handlerFunc)
    {
        m_handlerId = ++m_handlerIdCounter;
    }

    //Copy constructor
    GalaxyHandler(const GalaxyHandler& src) : m_handlerFunc(src.m_handlerFunc), m_handlerId(src.m_handlerId) {}

    //Move costructor
    GalaxyHandler(const GalaxyHandler&& src) : m_handlerFunc(std::move(src.m_handlerFunc)), m_handlerId(src.m_handlerId) {}

    //Copy assignment operator
    GalaxyHandler& operator=(const GalaxyHandler& src)
    {
        m_handlerFunc = src.m_handlerFunc;
        m_handlerId = src.m_handlerId;

        return *this;
    }

    //Move assignment operator
    GalaxyHandler& operator=(GalaxyHandler&& src)
    {
        std::swap(m_handlerFunc, src.m_handlerFunc);
        m_handlerId = src.m_handlerId;

        return *this;
    }

    bool operator==(const GalaxyHandler& other) const
    {
        return m_handlerId == other.m_handlerId;
    }

    operator bool() const
    {
        return m_handlerFunc;
    }

    handler_id_type id() const
    {
        return m_handlerId;
    }

private:
    handler m_handlerFunc; 
    handler_id_type m_handlerId;
    static std::atomic_uint m_handlerIdCounter;
};

template<typename... Args>
std::atomic_uint GalaxyHandler<Args...>::m_handlerIdCounter(0);

template<typename... Args>
class event
{
public:
    typedef GalaxyHandler<Args...> handler_type;

    event() {}

    //Copy constructor
    event(const event& src)
    {
        std::lock_guard<std::mutex> lock(src.m_handlersLocker);
        m_handlers = std::move(src.m_handlers);
    }

    //Copy assignment operator
    event& operator=(const event& src)
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);
        std::lock_guard<std::mutex> lock2(src.m_handlersLocker);

        m_handlers = src.m_handlers;

        return *this;
    }

    //Move assignment operator
    event& operator=(event&& src)
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);
        std::lock_guard<std::mutex> lock2(src.m_handlersLocker);

        std::swap(m_handlers, src.m_handlers);

        return *this;
    }

    typename handler_type::handler_id_type add(const handler_type& handler)
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);

        m_handlers.push_back(handler);

        return handler.id();
    }

    inline typename handler_type::handler_id_type add(const typename handler_type::handler& handler)
    {
        return add(handler_type(handler));
    }

    bool remove(const handler_type& handler)
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);

        auto it = std::find(m_handlers.begin(), m_handlers.end(), handler);
        if(it != m_handlers.end())
        {
            m_handlers.erase(it);
            return true;
        }
        return false;
    }

    bool remove_id(const typename handler_type::handler_id_type& handlerId)
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);

        auto it = std::find_if(m_handlers.begin(), m_handlers.end(), [handlerId](const handler_type& handler) { return handler.id() == handlerId; });
        if(it != m_handlers.end())
        {
            m_handlers.erase(it);
            return true;
        }

        return false;
    }

    void call(Args... params) const
    {
        handler_collection_type handlersCopy = get_handlers_copy();

        call_impl(handlersCopy, params...);
    }

    std::future<void> call_async(Args... params) const
    {
        return std::async(std::launch::async, [this](Args... asyncParams){ call(asyncParams...); }, params...);
    }

    inline void operator()(Args... params) const
    {
        call(params...);
    }

    inline typename handler_type::handler_if_type operator+=(const typename handler_type::handler& handler)
    {
        return add(handler);
    }

    inline bool operator-=(const handler_type& handler)
    {
        return remove(handler);
    }

protected:
    typedef std::list<GalaxyHandler> handler_collection_type;

    void call_impl(const handler_collection_type& handlers, Args... params) const
    {
        for(const auto& handler : handlers)
        {
            handler(params...);
        }
    }

    handler_collection_type get_handlers_copy() const
    {
        std::lock_guard<std::mutex> lock(m_handlersLocker);

        return m_handlers;
    }

private:
    handler_collection_type m_handlers;
    mutable std::mutex m_handlersLocker;
};

#endif