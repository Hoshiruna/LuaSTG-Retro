#include "core/FileSystemWatcher.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include "core/Logger.hpp"
#include <efsw/efsw.hpp>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

namespace core
{
    namespace
    {
        std::filesystem::path pathFromUtf8(const std::string_view path)
        {
            return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(path.data()), path.size()));
        }

        std::string pathToUtf8(const std::filesystem::path& path)
        {
            const auto text = path.generic_u8string();
            return { reinterpret_cast<const char*>(text.data()), text.size() };
        }

        class MessageQueueBasedFileSystemWatcher final : public implement::ReferenceCounted<IMessageQueueBasedFileSystemWatcher>, public efsw::FileWatchListener
        {
        public:
            bool next(FileNotifyInformation* const info) override
            {
                if(info == nullptr) {
                    return false;
                }
                std::lock_guard lock(m_mutex);
                if(m_notifications.empty()) {
                    return false;
                }
                *info = std::move(m_notifications.front());
                m_notifications.pop_front();
                return true;
            }

            bool open(const std::string_view path)
            {
                m_root = std::filesystem::absolute(pathFromUtf8(path)).lexically_normal();
                m_watcher = std::make_unique<efsw::FileWatcher>();
                const auto watch = m_watcher->addWatch(pathToUtf8(m_root), this, true);
                if(watch < 0) {
                    Logger::error("[core::FileSystemWatcher] Cannot watch '{}': {} ({})", path, efsw::Errors::Log::getLastErrorLog(), watch);
                    return false;
                }
                m_watcher->watch();
                return true;
            }

            void handleFileAction(efsw::WatchID, const std::string& directory, const std::string& filename, const efsw::Action action, const std::string& old_filename) override
            {
                try {
                    std::list<FileNotifyInformation> events;
                    if(action == efsw::Actions::Moved && !old_filename.empty()) {
                        events.push_back(notification(directory, old_filename, FileAction::renamed_old_name));
                        events.push_back(notification(directory, filename, FileAction::renamed_new_name));
                    } else {
                        FileAction translated{};
                        switch(action) {
                            case efsw::Actions::Add:
                            case efsw::Actions::Moved:
                                translated = FileAction::added;
                                break;
                            case efsw::Actions::Delete:
                                translated = FileAction::removed;
                                break;
                            case efsw::Actions::Modified:
                                translated = FileAction::modified;
                                break;
                            default:
                                return;
                        }
                        events.push_back(notification(directory, filename, translated));
                    }
                    // Enqueue both halves of a rename together.
                    std::lock_guard lock(m_mutex);
                    m_notifications.splice(m_notifications.end(), events);
                } catch(const std::exception& error) {
                    Logger::error("[core::FileSystemWatcher] Could not queue notification: {}", error.what());
                }
            }

            void handleMissedFileActions(efsw::WatchID, const std::string& directory) override
            {
                Logger::warn("[core::FileSystemWatcher] File notifications were lost in '{}'", directory);
            }

        private:
            FileNotifyInformation notification(const std::string& directory, const std::string& filename, const FileAction action) const
            {
                const auto absolute = (pathFromUtf8(directory) / pathFromUtf8(filename)).lexically_normal();
                const auto relative = absolute.lexically_relative(m_root);
                FileNotifyInformation result;
                IImmutableString::create(pathToUtf8(relative), &result.file_name);
                result.action = action;
                return result;
            }

            std::filesystem::path m_root;
            std::mutex m_mutex;
            std::list<FileNotifyInformation> m_notifications;
            // Destroy the worker before the queue, mutex, and listener base.
            std::unique_ptr<efsw::FileWatcher> m_watcher;
        };
    }

    bool IMessageQueueBasedFileSystemWatcher::create(std::string_view const& path, IMessageQueueBasedFileSystemWatcher** const object)
    {
        if(object == nullptr) {
            return false;
        }
        *object = nullptr;
        try {
            SmartReference<MessageQueueBasedFileSystemWatcher> watcher;
            watcher.attach(new MessageQueueBasedFileSystemWatcher);
            if(!watcher->open(path)) {
                return false;
            }
            *object = watcher.detach();
            return true;
        } catch(const std::exception& error) {
            Logger::error("[core::FileSystemWatcher] Cannot watch '{}': {}", path, error.what());
            return false;
        }
    }
}
