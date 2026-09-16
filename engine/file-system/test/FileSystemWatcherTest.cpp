#include "core/FileSystemWatcher.hpp"
#include "core/SmartReference.hpp"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace
{
    std::string utf8Path(const std::filesystem::path& path)
    {
        const auto text = path.generic_u8string();
        return { reinterpret_cast<const char*>(text.data()), text.size() };
    }

    class FileSystemWatcherTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            if(spdlog::default_logger() == nullptr) {
                spdlog::set_default_logger(spdlog::stdout_color_mt("watcher-test"));
            }
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("luastg-watcher-" + std::to_string(stamp));
            owns_root = std::filesystem::create_directory(root);
            ASSERT_TRUE(owns_root);
            ASSERT_TRUE(std::filesystem::create_directory(root / "nested"));
            ASSERT_TRUE(core::IMessageQueueBasedFileSystemWatcher::create(utf8Path(root), watcher.put()));
        }

        void TearDown() override
        {
            watcher.reset();
            std::error_code error;
            if(owns_root) {
                std::filesystem::remove_all(root, error);
            }
        }

        bool waitFor(const core::FileAction action, const std::string& path)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while(std::chrono::steady_clock::now() < deadline) {
                core::FileNotifyInformation notification;
                while(watcher->next(&notification)) {
                    if(notification.action == action && notification.file_name != nullptr && notification.file_name->view() == path) {
                        return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return false;
        }

        std::filesystem::path root;
        bool owns_root{};
        core::SmartReference<core::IMessageQueueBasedFileSystemWatcher> watcher;
    };

    TEST_F(FileSystemWatcherTest, ReportsRecursiveUnicodeCreateRenameAndDelete)
    {
        const std::filesystem::path original = std::filesystem::path("nested") / std::filesystem::path(u8"日本語-😀.txt");
        const std::filesystem::path renamed = std::filesystem::path("nested") / std::filesystem::path(u8"renamed-😀.txt");
        {
            std::ofstream stream(root / original);
            ASSERT_TRUE(stream.is_open());
            stream << "created";
        }
        ASSERT_TRUE(waitFor(core::FileAction::added, utf8Path(original)));
        std::filesystem::rename(root / original, root / renamed);
        ASSERT_TRUE(waitFor(core::FileAction::renamed_old_name, utf8Path(original)));
        ASSERT_TRUE(waitFor(core::FileAction::renamed_new_name, utf8Path(renamed)));
        ASSERT_TRUE(std::filesystem::remove(root / renamed));
        ASSERT_TRUE(waitFor(core::FileAction::removed, utf8Path(renamed)));
    }

    TEST_F(FileSystemWatcherTest, ReleasesWorkerWithPendingNotifications)
    {
        for(int i = 0; i < 32; ++i) {
            std::ofstream stream(root / (std::to_string(i) + ".txt"));
            ASSERT_TRUE(stream.is_open());
            stream << i;
        }
        watcher.reset();
        core::SmartReference<core::IMessageQueueBasedFileSystemWatcher> reopened;
        ASSERT_TRUE(core::IMessageQueueBasedFileSystemWatcher::create(utf8Path(root), reopened.put()));
    }
}
