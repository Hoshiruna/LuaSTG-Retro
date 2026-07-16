#include "GameResource/AsyncResourceLoader.hpp"
#include "core/FileSystem.hpp"
#include "utf8.hpp"

#include <cstring>
#include <filesystem>
#include <string_view>
#include <utility>

namespace luastg {
	namespace {
		bool readFile(std::string_view const path, core::SmartReference<core::IData>& data, std::string& error) {
			if (!core::FileSystemManager::readFile(path, data.put())) {
				error = "failed to read file";
				return false;
			}
			return true;
		}

		bool isTerminal(AsyncResourceJobState const state) {
			return state == AsyncResourceJobState::Done
				|| state == AsyncResourceJobState::Failed
				|| state == AsyncResourceJobState::Cancelled;
		}

		bool readHgeFontTexturePath(core::IData* const data, std::string& texture) {
			if (!data) {
				return false;
			}

			std::string_view view(static_cast<char const*>(data->data()), data->size());
			while (!view.empty()) {
				size_t const pos = view.find_first_of('\n');
				std::string_view line = pos == std::string_view::npos ? view : view.substr(0, pos);
				if (!line.empty() && line.back() == '\r') {
					line = line.substr(0, line.size() - 1);
				}
				if (line.starts_with("Bitmap=")) {
					texture.assign(line.substr(7));
					return !texture.empty();
				}
				if (pos == std::string_view::npos) {
					break;
				}
				view = view.substr(pos + 1);
			}
			return false;
		}

		bool readRelatedFile(std::string_view const base_path, std::string_view const file_path,
			core::SmartReference<core::IData>& data, std::string& resolved_path, std::string& error) {
			if (file_path.empty()) {
				error = "invalid file path";
				return false;
			}

			resolved_path.assign(file_path);
			if (readFile(resolved_path, data, error)) {
				return true;
			}

			std::filesystem::path wide_path(utf8::to_wstring(base_path));
			wide_path.remove_filename();
			wide_path /= utf8::to_wstring(file_path);
			resolved_path = utf8::to_string(wide_path.wstring());
			if (readFile(resolved_path, data, error)) {
				return true;
			}

			error = "failed to read related file";
			return false;
		}
	}

	AsyncResourceJob::AsyncResourceJob(std::string_view const path)
		: m_kind(AsyncResourceJobKind::FileRead)
		, m_file_path(path)
		, m_debug_files{ std::string(path) } {
	}

	AsyncResourceJob::AsyncResourceJob(AsyncResourceRequest request)
		: m_kind(AsyncResourceJobKind::Resource)
		, m_request(std::move(request)) {
		auto const add_file = [this](std::string_view const path) {
			if (!path.empty()) {
				m_debug_files.emplace_back(path);
			}
		};

		switch (m_request.type) {
		case AsyncResourceRequestType::Texture:
		case AsyncResourceRequestType::Video:
		case AsyncResourceRequestType::Sound:
		case AsyncResourceRequestType::Music:
		case AsyncResourceRequestType::FX:
		case AsyncResourceRequestType::Model:
			add_file(m_request.path);
			break;

		case AsyncResourceRequestType::Particle:
			if (!m_request.has_particle_info) {
				add_file(m_request.path);
			}
			break;

		case AsyncResourceRequestType::SpriteFont:
			add_file(m_request.path);
			if (m_request.has_texture_path) {
				add_file(m_request.texture_path);
			}
			break;

		case AsyncResourceRequestType::TrueTypeFont:
			if (m_request.fonts.empty()) {
				add_file(m_request.path);
			}
			else {
				for (size_t i = 0; i < m_request.fonts.size() && i < m_request.font_sources.size(); ++i) {
					if (!m_request.fonts[i].is_buffer) {
						add_file(m_request.font_sources[i]);
					}
				}
			}
			break;

		case AsyncResourceRequestType::Sprite:
		case AsyncResourceRequestType::Animation:
			break;
		}
	}

	AsyncResourceJobKind AsyncResourceJob::getKind() const {
		std::lock_guard const lock(m_mutex);
		return m_kind;
	}

	AsyncResourceJobState AsyncResourceJob::getState() const {
		std::lock_guard const lock(m_mutex);
		return m_state;
	}

	char const* AsyncResourceJob::getStateName() const {
		switch (getState()) {
		case AsyncResourceJobState::Queued: return "queued";
		case AsyncResourceJobState::Running: return "running";
		case AsyncResourceJobState::Ready: return "ready";
		case AsyncResourceJobState::Done: return "done";
		case AsyncResourceJobState::Failed: return "failed";
		case AsyncResourceJobState::Cancelled: return "cancelled";
		default: return "failed";
		}
	}

	bool AsyncResourceJob::isFinished() const {
		std::lock_guard const lock(m_mutex);
		return isTerminal(m_state);
	}

	bool AsyncResourceJob::cancel() {
		std::lock_guard const lock(m_mutex);
		if (isTerminal(m_state)) {
			return false;
		}
		m_cancel_requested = true;
		if (m_state == AsyncResourceJobState::Queued || m_state == AsyncResourceJobState::Ready) {
			m_state = AsyncResourceJobState::Cancelled;
		}
		return true;
	}

	std::string AsyncResourceJob::getError() const {
		std::lock_guard const lock(m_mutex);
		return m_error;
	}

	core::SmartReference<core::IData> AsyncResourceJob::getFileData() const {
		std::lock_guard const lock(m_mutex);
		return m_data;
	}

	AsyncResourceJobDebugInfo AsyncResourceJob::getDebugInfo() const {
		std::lock_guard const lock(m_mutex);
		AsyncResourceJobDebugInfo info;
		info.kind = m_kind;
		info.state = m_state;
		info.resource_type = m_request.type;
		info.pool_id = m_request.pool_id;
		info.resource_name = m_request.name;
		info.files = m_debug_files;
		info.error = m_error;
		return info;
	}

	bool AsyncResourceJob::isCancelled() const {
		std::lock_guard const lock(m_mutex);
		return m_cancel_requested || m_state == AsyncResourceJobState::Cancelled;
	}

	void AsyncResourceJob::setState(AsyncResourceJobState const state) {
		std::lock_guard const lock(m_mutex);
		if (m_cancel_requested && !isTerminal(state)) {
			m_state = AsyncResourceJobState::Cancelled;
		}
		else {
			m_state = state;
		}
	}

	void AsyncResourceJob::fail(std::string_view const message) {
		std::lock_guard const lock(m_mutex);
		if (m_cancel_requested) {
			m_state = AsyncResourceJobState::Cancelled;
			return;
		}
		m_error.assign(message);
		m_state = AsyncResourceJobState::Failed;
	}

	void AsyncResourceJob::finish() {
		std::lock_guard const lock(m_mutex);
		m_state = m_cancel_requested ? AsyncResourceJobState::Cancelled : AsyncResourceJobState::Done;
	}

	void AsyncResourceJob::setSecondaryDebugFile(std::string_view const path) {
		std::lock_guard const lock(m_mutex);
		if (m_debug_files.size() < 2) {
			m_debug_files.emplace_back(path);
		}
		else {
			m_debug_files[1].assign(path);
		}
	}

	AsyncResourceLoader::AsyncResourceLoader() {
		m_worker = std::thread(&AsyncResourceLoader::workerMain, this);
	}

	AsyncResourceLoader::~AsyncResourceLoader() {
		stop();
	}

	void AsyncResourceLoader::stop() noexcept {
		cancelAll();
		{
			std::lock_guard const lock(m_mutex);
			m_exit = true;
		}
		m_cv.notify_one();
		if (m_worker.joinable()) {
			m_worker.join();
		}
	}

	std::shared_ptr<AsyncResourceJob> AsyncResourceLoader::submitFileRead(std::string_view const path) {
		auto job = std::shared_ptr<AsyncResourceJob>(new AsyncResourceJob(path));
		{
			std::lock_guard const lock(m_mutex);
			if (m_exit) {
				job->fail("async resource loader has stopped");
				return job;
			}
			m_jobs.emplace_back(job);
			m_queue.emplace_back(job);
		}
		m_cv.notify_one();
		return job;
	}

	std::shared_ptr<AsyncResourceJob> AsyncResourceLoader::submitResource(AsyncResourceRequest request) {
		auto job = std::shared_ptr<AsyncResourceJob>(new AsyncResourceJob(std::move(request)));
		{
			std::lock_guard const lock(m_mutex);
			if (m_exit) {
				job->fail("async resource loader has stopped");
				return job;
			}
			m_jobs.emplace_back(job);
			m_queue.emplace_back(job);
		}
		m_cv.notify_one();
		return job;
	}

	std::shared_ptr<AsyncResourceJob> AsyncResourceLoader::submitFailedResource(AsyncResourceRequest request, std::string_view const message) {
		auto job = std::shared_ptr<AsyncResourceJob>(new AsyncResourceJob(std::move(request)));
		job->fail(message);
		{
			std::lock_guard const lock(m_mutex);
			addHistory(job->getDebugInfo());
		}
		return job;
	}

	std::vector<AsyncResourceJobDebugInfo> AsyncResourceLoader::getDebugSnapshot() {
		std::lock_guard const lock(m_mutex);
		std::vector<AsyncResourceJobDebugInfo> result;
		result.reserve(m_history.size() + m_jobs.size());
		for (auto const& job : m_jobs) {
			if (job) {
				result.emplace_back(job->getDebugInfo());
			}
		}
		result.insert(result.end(), m_history.rbegin(), m_history.rend());
		return result;
	}

	void AsyncResourceLoader::cancel(ResourcePoolId const pool_id) noexcept {
		std::lock_guard const lock(m_mutex);
		for (auto& job : m_jobs) {
			if (job && job->getKind() == AsyncResourceJobKind::Resource && job->m_request.pool_id == pool_id) {
				(void)job->cancel();
			}
		}
	}

	void AsyncResourceLoader::cancelAll() noexcept {
		std::lock_guard const lock(m_mutex);
		for (auto& job : m_jobs) {
			if (job) {
				(void)job->cancel();
			}
		}
	}

	void AsyncResourceLoader::workerMain() {
		for (;;) {
			std::shared_ptr<AsyncResourceJob> job;
			{
				std::unique_lock lock(m_mutex);
				m_cv.wait(lock, [this] { return m_exit || !m_queue.empty(); });
				if (m_exit) {
					break;
				}
				job = std::move(m_queue.front());
				m_queue.pop_front();
			}

			if (!job) {
				continue;
			}
			if (job->isCancelled()) {
				job->setState(AsyncResourceJobState::Cancelled);
				continue;
			}
			process(*job);
			if (job->getKind() == AsyncResourceJobKind::Resource && job->getState() == AsyncResourceJobState::Ready) {
				queueReady(job);
			}
		}
	}

	void AsyncResourceLoader::process(AsyncResourceJob& job) {
		job.setState(AsyncResourceJobState::Running);
		if (job.isCancelled()) {
			job.setState(AsyncResourceJobState::Cancelled);
			return;
		}

		std::string error;
		if (job.m_kind == AsyncResourceJobKind::FileRead) {
			if (!readFile(job.m_file_path, job.m_data, error)) {
				job.fail(error);
				return;
			}
			job.finish();
			return;
		}

		auto& request = job.m_request;
		switch (request.type) {
		case AsyncResourceRequestType::Texture:
		case AsyncResourceRequestType::Video:
		case AsyncResourceRequestType::FX:
		case AsyncResourceRequestType::Model:
			if (!readFile(request.path, job.m_data, error)) {
				job.fail(error);
				return;
			}
			break;

		case AsyncResourceRequestType::Sound:
		case AsyncResourceRequestType::Music:
			if (!readFile(request.path, job.m_data, error)) {
				job.fail(error);
				return;
			}
			if (!core::IAudioDecoder::create(job.m_data.get(), job.m_audio_decoder.put())) {
				job.fail("failed to decode audio");
				return;
			}
			break;

		case AsyncResourceRequestType::Particle:
			if (!request.has_particle_info) {
				if (!readFile(request.path, job.m_data, error)) {
					job.fail(error);
					return;
				}
				if (job.m_data->size() != sizeof(hgeParticleSystemInfo)) {
					job.fail("invalid particle file");
					return;
				}
				std::memcpy(&job.m_particle_info, job.m_data->data(), sizeof(hgeParticleSystemInfo));
			}
			break;

		case AsyncResourceRequestType::SpriteFont:
			if (!readFile(request.path, job.m_data, error)) {
				job.fail(error);
				return;
			}
			if (request.has_texture_path) {
				std::string texture_path = request.texture_path;
				if (!readRelatedFile(request.path, texture_path, job.m_texture_data, request.texture_path, error)) {
					job.fail(error);
					return;
				}
				job.setSecondaryDebugFile(request.texture_path);
			}
			else {
				std::string texture_path;
				if (!readHgeFontTexturePath(job.m_data.get(), texture_path)) {
					job.fail("invalid font file");
					return;
				}
				if (!readRelatedFile(request.path, texture_path, job.m_texture_data, request.texture_path, error)) {
					job.fail(error);
					return;
				}
				job.setSecondaryDebugFile(request.texture_path);
			}
			break;

		case AsyncResourceRequestType::TrueTypeFont:
			if (request.fonts.empty()) {
				if (!readFile(request.path, job.m_data, error)) {
					job.fail(error);
					return;
				}
			}
			else {
				job.m_font_data.resize(request.fonts.size());
				for (size_t i = 0; i < request.fonts.size(); ++i) {
					if (request.fonts[i].is_buffer || i >= request.font_sources.size()) {
						continue;
					}
					if (!core::FileSystemManager::readFile(request.font_sources[i], job.m_font_data[i].put())) {
						job.fail("failed to read font file");
						return;
					}
				}
			}
			break;

		case AsyncResourceRequestType::Sprite:
		case AsyncResourceRequestType::Animation:
			break;
		}

		if (job.isCancelled()) {
			job.setState(AsyncResourceJobState::Cancelled);
			return;
		}
		job.setState(AsyncResourceJobState::Ready);
	}

	void AsyncResourceLoader::queueReady(std::shared_ptr<AsyncResourceJob> const& job) {
		std::lock_guard const lock(m_mutex);
		m_ready.emplace_back(job);
	}

	void AsyncResourceLoader::update(ResourceMgr& manager, size_t const max_count) {
		size_t count = 0;
		while (count < max_count) {
			std::shared_ptr<AsyncResourceJob> job;
			{
				std::lock_guard const lock(m_mutex);
				if (m_ready.empty()) {
					break;
				}
				job = std::move(m_ready.front());
				m_ready.pop_front();
			}
			if (!job || job->isFinished()) {
				continue;
			}
			if (job->isCancelled()) {
				job->setState(AsyncResourceJobState::Cancelled);
				continue;
			}
			if (finalize(manager, *job)) {
				job->finish();
			}
			++count;
		}

		std::lock_guard const lock(m_mutex);
		for (auto it = m_jobs.begin(); it != m_jobs.end();) {
			if (!*it) {
				it = m_jobs.erase(it);
			}
			else if (it->use_count() == 1 && (*it)->isFinished()) {
				addHistory((*it)->getDebugInfo());
				it = m_jobs.erase(it);
			}
			else {
				++it;
			}
		}
	}

	void AsyncResourceLoader::addHistory(AsyncResourceJobDebugInfo info) {
		constexpr size_t history_limit = 128;
		if (m_history.size() == history_limit) {
			m_history.pop_front();
		}
		m_history.emplace_back(std::move(info));
	}

	bool AsyncResourceLoader::finalize(ResourceMgr& manager, AsyncResourceJob& job) {
		auto& request = job.m_request;
		auto* pool = manager.GetResourcePool(request.pool_id);
		if (!pool) {
			job.fail("can't load resource at this time");
			return false;
		}
		if (manager.GetResourcePoolGeneration(request.pool_id) != request.pool_generation) {
			job.fail("resource pool was cleared");
			return false;
		}

		switch (request.type) {
		case AsyncResourceRequestType::Texture:
			if (!pool->LoadTexture(request.name.c_str(), job.m_data.get(), request.path.c_str(), request.mipmap)) {
				job.fail("failed to load texture");
				return false;
			}
			break;

		case AsyncResourceRequestType::Video:
			{
				core::SmartReference<core::IVideoDecoder> decoder;
				if (!core::IVideoDecoder::create(job.m_data.get(), decoder.put())) {
					job.fail("failed to decode video");
					return false;
				}
				if (!pool->LoadVideo(request.name.c_str(), decoder.get(), request.loop)) {
					job.fail("failed to load video");
					return false;
				}
			}
			break;

		case AsyncResourceRequestType::Sprite:
			if (!pool->CreateSprite(request.name.c_str(), request.texture.get(),
				request.x, request.y, request.w, request.h, request.a, request.b, request.rect)) {
				job.fail("failed to create sprite");
				return false;
			}
			break;

		case AsyncResourceRequestType::Animation:
			if (request.animation_uses_sprite_list) {
				if (!pool->CreateAnimation(request.name.c_str(), request.sprites, request.interval, request.a, request.b, request.rect)) {
					job.fail("failed to create animation");
					return false;
				}
			}
			else if (!pool->CreateAnimation(request.name.c_str(), request.texture.get(),
				request.x, request.y, request.w, request.h, request.columns, request.rows, request.interval,
				request.a, request.b, request.rect)) {
				job.fail("failed to create animation");
				return false;
			}
			break;

		case AsyncResourceRequestType::Particle:
			if (request.has_particle_info) {
				if (!pool->LoadParticle(request.name.c_str(), request.particle_info, request.sprite.get(),
					request.a, request.b, request.rect)) {
					job.fail("failed to load particle");
					return false;
				}
			}
			else if (!pool->LoadParticle(request.name.c_str(), job.m_particle_info, request.sprite.get(),
				request.a, request.b, request.rect)) {
				job.fail("failed to load particle");
				return false;
			}
			break;

		case AsyncResourceRequestType::Sound:
			if (!pool->LoadSoundEffect(request.name.c_str(), job.m_audio_decoder.get(), request.path.c_str())) {
				job.fail("failed to load sound");
				return false;
			}
			break;

		case AsyncResourceRequestType::Music:
			if (!pool->LoadMusic(request.name.c_str(), job.m_audio_decoder.get(), request.path.c_str(),
				request.loop_start, request.loop_end, request.once_decode)) {
				job.fail("failed to load music");
				return false;
			}
			break;

		case AsyncResourceRequestType::SpriteFont:
			if (request.has_texture_path) {
				if (!pool->LoadSpriteFont(request.name.c_str(), job.m_data.get(), request.path.c_str(),
					request.texture_path.c_str(), job.m_texture_data.get(), request.mipmap)) {
					job.fail("failed to load font");
					return false;
				}
			}
			else if (!pool->LoadSpriteFont(request.name.c_str(), job.m_data.get(), request.path.c_str(),
				job.m_texture_data.get(), request.texture_path.c_str(), request.mipmap)) {
				job.fail("failed to load font");
				return false;
			}
			break;

		case AsyncResourceRequestType::TrueTypeFont:
			if (request.fonts.empty()) {
				bool result = false;
				if (job.m_data) {
					result = pool->LoadTTFFont(request.name.c_str(), job.m_data.get(), request.font_width, request.font_height);
				}
				else {
					result = pool->LoadTTFFont(request.name.c_str(), request.path.c_str(), request.font_width, request.font_height);
				}
				if (!result) {
					job.fail("failed to load TTF font");
					return false;
				}
			}
			else {
				auto fonts = request.fonts;
				for (size_t i = 0; i < fonts.size(); ++i) {
					if (i < job.m_font_data.size() && job.m_font_data[i]) {
						fonts[i].source = core::StringView(static_cast<char const*>(job.m_font_data[i]->data()), job.m_font_data[i]->size());
						fonts[i].is_buffer = true;
						fonts[i].is_force_to_file = false;
					}
					else if (i < request.font_sources.size()) {
						fonts[i].source = request.font_sources[i];
					}
				}
				if (!pool->LoadTrueTypeFont(request.name.c_str(), fonts.data(), fonts.size())) {
					job.fail("failed to load TrueType font");
					return false;
				}
			}
			break;

		case AsyncResourceRequestType::FX:
			if (!pool->LoadFXFromSource(request.name.c_str(),
				std::string_view(static_cast<char const*>(job.m_data->data()), job.m_data->size()),
				request.path.c_str())) {
				job.fail("failed to load shader");
				return false;
			}
			break;

		case AsyncResourceRequestType::Model:
			if (!pool->LoadModel(request.name.c_str(), request.path.c_str())) {
				job.fail("failed to load model");
				return false;
			}
			break;
		}
		return true;
	}

}
