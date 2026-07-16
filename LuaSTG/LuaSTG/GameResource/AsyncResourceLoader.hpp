#pragma once
#include "GameResource/ResourceManager.h"
#include "core/AudioDecoder.hpp"
#include "core/Data.hpp"
#include "core/SmartReference.hpp"
#include "core/VideoDecoder.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace luastg {

	enum class AsyncResourceJobState {
		Queued,
		Running,
		Ready,
		Done,
		Failed,
		Cancelled,
	};

	enum class AsyncResourceJobKind {
		FileRead,
		Resource,
	};

	enum class AsyncResourceRequestType {
		Texture,
		Video,
		Sprite,
		Animation,
		Particle,
		Sound,
		Music,
		SpriteFont,
		TrueTypeFont,
		FX,
		Model,
	};

	struct AsyncResourceRequest {
		AsyncResourceRequestType type{ AsyncResourceRequestType::Texture };
		ResourcePoolType pool_type{ ResourcePoolType::None };
		size_t pool_generation{};

		std::string name;
		std::string path;
		std::string texture_name;
		std::string image_name;
		std::string texture_path;
		std::vector<std::string> sprite_names;
		std::vector<core::Graphics::TrueTypeFontInfo> fonts;
		std::vector<std::string> font_sources;

		double x{};
		double y{};
		double w{};
		double h{};
		double a{};
		double b{};
		double loop_start{};
		double loop_end{};
		float font_width{};
		float font_height{};
		int columns{};
		int rows{};
		int interval{};
		bool mipmap{ true };
		bool loop{};
		bool rect{};
		bool once_decode{};
		bool has_texture_path{};
		bool has_particle_info{};
		bool animation_uses_sprite_list{};
		hgeParticleSystemInfo particle_info{};
	};

	struct AsyncResourceJobDebugInfo {
		AsyncResourceJobKind kind{ AsyncResourceJobKind::FileRead };
		AsyncResourceJobState state{ AsyncResourceJobState::Queued };
		AsyncResourceRequestType resource_type{ AsyncResourceRequestType::Texture };
		ResourcePoolType pool_type{ ResourcePoolType::None };
		std::string resource_name;
		std::vector<std::string> files;
		std::string error;
	};

	class AsyncResourceJob {
	public:
		AsyncResourceJobKind getKind() const;
		AsyncResourceJobState getState() const;
		char const* getStateName() const;
		bool isFinished() const;
		bool cancel();
		std::string getError() const;
		core::SmartReference<core::IData> getFileData() const;
		AsyncResourceJobDebugInfo getDebugInfo() const;

	private:
		friend class AsyncResourceLoader;

		explicit AsyncResourceJob(std::string_view path);
		explicit AsyncResourceJob(AsyncResourceRequest request);

		bool isCancelled() const;
		void setState(AsyncResourceJobState state);
		void fail(std::string_view message);
		void finish();
		void setSecondaryDebugFile(std::string_view path);

	private:
		mutable std::mutex m_mutex;
		AsyncResourceJobKind m_kind{ AsyncResourceJobKind::FileRead };
		AsyncResourceJobState m_state{ AsyncResourceJobState::Queued };
		bool m_cancel_requested{};
		std::string m_error;
		std::string m_file_path;
		AsyncResourceRequest m_request;
		std::vector<std::string> m_debug_files;

		core::SmartReference<core::IData> m_data;
		core::SmartReference<core::IData> m_texture_data;
		core::SmartReference<core::IAudioDecoder> m_audio_decoder;
		hgeParticleSystemInfo m_particle_info{};
		std::vector<core::SmartReference<core::IData>> m_font_data;
	};

	class AsyncResourceLoader {
	public:
		AsyncResourceLoader();
		~AsyncResourceLoader();

		std::shared_ptr<AsyncResourceJob> submitFileRead(std::string_view path);
		std::shared_ptr<AsyncResourceJob> submitResource(AsyncResourceRequest request);
		std::shared_ptr<AsyncResourceJob> submitFailedResource(AsyncResourceRequest request, std::string_view message);
		void update(ResourceMgr& manager, size_t max_count);
		void cancel(ResourcePoolType pool_type) noexcept;
		void cancelAll() noexcept;
		void stop() noexcept;
		std::vector<AsyncResourceJobDebugInfo> getDebugSnapshot();

	private:
		void workerMain();
		void process(AsyncResourceJob& job);
		bool finalize(ResourceMgr& manager, AsyncResourceJob& job);
		void queueReady(std::shared_ptr<AsyncResourceJob> const& job);
		void addHistory(AsyncResourceJobDebugInfo info);

	private:
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::deque<std::shared_ptr<AsyncResourceJob>> m_queue;
		std::deque<std::shared_ptr<AsyncResourceJob>> m_ready;
		std::vector<std::shared_ptr<AsyncResourceJob>> m_jobs;
		std::deque<AsyncResourceJobDebugInfo> m_history;
		std::thread m_worker;
		bool m_exit{};
	};

}
