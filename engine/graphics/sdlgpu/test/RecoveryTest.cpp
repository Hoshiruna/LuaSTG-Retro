#include "Recovery.hpp"
#include "Device.hpp"
#include <gtest/gtest.h>

namespace core::Graphics::SDLGPU
{
    TEST(Recovery, AttemptBudgetIsSharedAcrossDevices)
    {
        std::atomic_bool attempted{};
        RecoveryPolicy first(attempted), second(attempted);
        EXPECT_EQ(first.state(), RecoveryState::Healthy);
        EXPECT_FALSE(first.begin());
        EXPECT_FALSE(attempted.load());
        first.request();
        EXPECT_TRUE(first.begin());
        EXPECT_TRUE(first.complete());
        second.request();
        EXPECT_FALSE(second.begin());
        EXPECT_EQ(second.state(), RecoveryState::Fatal);
    }

    TEST(Recovery, ListenerFailureCannotCompleteRecovery)
    {
        std::atomic_bool attempted{};
        RecoveryPolicy policy(attempted);
        EXPECT_FALSE(policy.complete());
        policy.request();
        EXPECT_FALSE(policy.complete());
        EXPECT_TRUE(policy.begin());
        policy.fatal();
        EXPECT_FALSE(policy.complete());
        EXPECT_EQ(policy.state(), RecoveryState::Fatal);
        policy.request();
        EXPECT_FALSE(policy.begin());
        EXPECT_TRUE(attempted.load());
    }

    TEST(Recovery, ReleaseAndRebuildFollowDependencyStages)
    {
        RecoveryRegistry registry;
        std::vector<int> events;
        registry.add(RecoveryStage::Renderer, "renderer", [&] { events.push_back(-2); }, [&] { events.push_back(2); });
        registry.add(RecoveryStage::Storage, "texture", [&] { events.push_back(-1); }, [&] { events.push_back(1); });
        registry.release();
        registry.rebuild();
        EXPECT_EQ(events, (std::vector<int>{ -2, -1, 1, 2 }));
    }

    TEST(Recovery, DestroyedRegistrationIsNotCalled)
    {
        RecoveryRegistry registry;
        int calls{};
        {
            RecoveryRegistration registration;
            registration.attach(registry, RecoveryStage::Storage, "temporary", [&] { ++calls; }, [&] { ++calls; });
        }
        registry.release();
        registry.rebuild();
        EXPECT_EQ(calls, 0);
    }

    TEST(Recovery, PartialRebuildCanReleaseAllLiveResources)
    {
        RecoveryRegistry registry;
        bool texture{}, renderer{};
        registry.add(RecoveryStage::Storage, "texture", [&] { texture = false; }, [&] { texture = true; });
        registry.add(RecoveryStage::Renderer, "renderer", [&] { renderer = false; }, [&] {
            renderer = true;
            throw std::runtime_error("injected"); });
        try {
            registry.rebuild();
            FAIL() << "Expected reconstruction failure";
        } catch(const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("Rebuild renderer: injected"), std::string::npos);
        }
        registry.release();
        EXPECT_FALSE(texture);
        EXPECT_FALSE(renderer);
    }

    TEST(Recovery, OperationalErrorsHaveDistinctType)
    {
        const GpuOperationError gpu("submission failed");
        const std::runtime_error asset("missing shader");
        const std::exception& gpu_base = gpu;
        const std::exception& asset_base = asset;
        EXPECT_NE(dynamic_cast<const GpuOperationError*>(&gpu_base), nullptr);
        EXPECT_EQ(dynamic_cast<const GpuOperationError*>(&asset_base), nullptr);
    }
}
