#include "GameResource/ResourceManager.h"
#ifdef USING_DEAR_IMGUI
#include "GameResource/AsyncResourceLoader.hpp"
#include "imgui.h"
#endif

static std::string bytes_count_to_string(unsigned long long size)
{
	int count = 0;
	char buffer[64] = {};
	if (size < 1024) // B
	{
		count = std::snprintf(buffer, 64, "%u B", (unsigned int)size);
	}
	else if (size < (1024 * 1024)) // KB
	{
		count = std::snprintf(buffer, 64, "%.2f KB", (double)size / 1024.0);
	}
	else if (size < (1024 * 1024 * 1024)) // MB
	{
		count = std::snprintf(buffer, 64, "%.2f MB", (double)size / 1048576.0);
	}
	else // GB
	{
		count = std::snprintf(buffer, 64, "%.2f GB", (double)size / 1073741824.0);
	}
	return std::string(buffer, count);
}

#ifdef USING_DEAR_IMGUI
static char const* async_resource_pool_name(luastg::ResourcePoolType const type)
{
	switch (type)
	{
	case luastg::ResourcePoolType::Global:
		return "Global";
	case luastg::ResourcePoolType::Stage:
		return "Stage";
	default:
		return "-";
	}
}

static char const* async_resource_type_name(luastg::AsyncResourceJobDebugInfo const& info)
{
	if (info.kind == luastg::AsyncResourceJobKind::FileRead)
	{
		return "File Read";
	}

	switch (info.resource_type)
	{
	case luastg::AsyncResourceRequestType::Texture:
		return "Texture";
	case luastg::AsyncResourceRequestType::Video:
		return "Video";
	case luastg::AsyncResourceRequestType::Sprite:
		return "Sprite";
	case luastg::AsyncResourceRequestType::Animation:
		return "Animation";
	case luastg::AsyncResourceRequestType::Particle:
		return "Particle";
	case luastg::AsyncResourceRequestType::Sound:
		return "Sound";
	case luastg::AsyncResourceRequestType::Music:
		return "Music";
	case luastg::AsyncResourceRequestType::SpriteFont:
		return "Sprite Font";
	case luastg::AsyncResourceRequestType::TrueTypeFont:
		return "TrueType Font";
	case luastg::AsyncResourceRequestType::FX:
		return "Post Effect";
	case luastg::AsyncResourceRequestType::Model:
		return "Model";
	default:
		return "Unknown";
	}
}

static char const* async_resource_state_name(luastg::AsyncResourceJobState const state)
{
	switch (state)
	{
	case luastg::AsyncResourceJobState::Queued:
		return "Queued";
	case luastg::AsyncResourceJobState::Running:
		return "Reading";
	case luastg::AsyncResourceJobState::Ready:
		return "Ready";
	case luastg::AsyncResourceJobState::Done:
		return "Done";
	case luastg::AsyncResourceJobState::Failed:
		return "Failed";
	case luastg::AsyncResourceJobState::Cancelled:
		return "Cancelled";
	default:
		return "Unknown";
	}
}

static ImVec4 async_resource_state_color(luastg::AsyncResourceJobState const state)
{
	switch (state)
	{
	case luastg::AsyncResourceJobState::Queued:
		return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
	case luastg::AsyncResourceJobState::Running:
		return ImVec4(1.0f, 0.75f, 0.25f, 1.0f);
	case luastg::AsyncResourceJobState::Ready:
		return ImVec4(0.35f, 0.7f, 1.0f, 1.0f);
	case luastg::AsyncResourceJobState::Done:
		return ImVec4(0.35f, 0.9f, 0.45f, 1.0f);
	case luastg::AsyncResourceJobState::Failed:
		return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
	case luastg::AsyncResourceJobState::Cancelled:
		return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	default:
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

static bool async_resource_state_finished(luastg::AsyncResourceJobState const state)
{
	return state == luastg::AsyncResourceJobState::Done
		|| state == luastg::AsyncResourceJobState::Failed
		|| state == luastg::AsyncResourceJobState::Cancelled;
}
#endif

namespace luastg
{
	void ResourceMgr::ShowResourceManagerDebugWindow(bool* p_open)
	{
	#ifdef USING_DEAR_IMGUI
		if (ImGui::Begin("Resource Manager##lstg.ResourceManager", p_open))
		{
			static int timer = 0;

			static int current_pool = 0;
			static char const* const pool_names[] = {
				"Global",
				"Stage",
			};
			ImGui::Combo("Resource Set", &current_pool, pool_names, 2);

			ResourcePool* p_pool = nullptr;
			switch (current_pool)
			{
			case 0:
				p_pool = &m_GlobalResourcePool;
				break;
			case 1:
				p_pool = &m_StageResourcePool;
				break;
			default:
				break;
			}

			auto draw_preview_scaling = [](float& scale) -> void
			{
				ImGui::PushID(&scale);
				ImGui::SliderFloat("##SliderFloat", &scale, 0.25f, 4.0f);
				ImGui::SameLine();
				if (ImGui::Button("Reset##Button"))
				{
					scale = 1.0f;
				}
				ImGui::SameLine();
				ImGui::Text("Preview Scaling");
				ImGui::PopID();
			};
			auto draw_texture0 = [](core::Graphics::ITexture2D* p_tex, float scale) -> void
			{
				auto const size = p_tex->getSize();
				ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1.0);
				ImGui::Image(
					reinterpret_cast<size_t>(p_tex->getNativeHandle()),
					ImVec2(scale * (float)size.x, scale * (float)size.y),
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f));
				ImGui::PopStyleVar();
			};
			auto draw_texture = [](IResourceTexture* p_res, bool show_info, float scale) -> void
			{
				auto const size = p_res->GetTexture()->getSize();
				if (show_info)
				{
					ImGui::Text("Size: %u x %u", size.x, size.y);
					ImGui::Text("RenderTarget: %s", p_res->IsRenderTarget() ? "Yes" : "Not");
					ImGui::Text("Dynamic: %s", p_res->IsRenderTarget() ? "Yes" : "Not");
					unsigned long long mem_usage = size.x * size.y * 4;
					ImGui::Text("Adapter Memory Usage (Approximate): %s", bytes_count_to_string(mem_usage).c_str());
				}
				ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1.0);
				ImGui::Image(
					reinterpret_cast<size_t>(p_res->GetTexture()->getNativeHandle()),
					ImVec2(scale * (float)size.x, scale * (float)size.y),
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f));
				ImGui::PopStyleVar();
			};
			auto draw_sprite = [](core::Graphics::ISprite* p_res, bool show_info, bool focus, float scale) -> void {
				auto color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
				if (focus)
				{
					color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
				}
				auto* p_tex = p_res->getTexture();
				auto tex_size = p_tex->getSize();
				auto rc = p_res->getTextureRect();
				auto cp = p_res->getTextureCenter() - rc.a;
				if (show_info)
				{
					ImGui::Text("Pos: %.2f x %.2f", rc.a.x, rc.a.y);
					ImGui::Text("Size: %.2f x %.2f", rc.b.x - rc.a.x, rc.b.y - rc.a.y);
					ImGui::Text("Center: %.2f x %.2f", cp.x, cp.y);
					ImGui::Text("Units Per Pixel: %.4f", p_res->getUnitsPerPixel());
				}
				ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1.0);
				ImGui::PushStyleColor(ImGuiCol_Border, color);
				ImGui::Image(
					reinterpret_cast<size_t>(p_tex->getNativeHandle()),
					ImVec2(scale * (rc.b.x - rc.a.x), scale * (rc.b.y - rc.a.y)),
					ImVec2(rc.a.x / (float)tex_size.x, rc.a.y / (float)tex_size.y),
					ImVec2(rc.b.x / (float)tex_size.x, rc.b.y / (float)tex_size.y));
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();
			};
			
			if (p_pool)
			{
				if (ImGui::BeginTabBar("##lstg.ResourceManager"))
				{
					if (ImGui::BeginTabItem("Async Loading"))
					{
						static bool show_finished = true;
						static bool selected_pool_only = true;
						static ImGuiTextFilter filter;

						ImGui::Checkbox("Show Finished", &show_finished);
						ImGui::SameLine();
						ImGui::Checkbox("Selected Pool Only", &selected_pool_only);
						filter.Draw("Filter");

						auto const selected_pool = current_pool == 0 ? ResourcePoolType::Global : ResourcePoolType::Stage;
						auto const jobs = m_AsyncLoader->getDebugSnapshot();
						size_t visible_count = 0;
						size_t active_count = 0;
						size_t failed_count = 0;
						for (auto const& job : jobs)
						{
							if (selected_pool_only && job.pool_type != selected_pool)
							{
								continue;
							}
							if (!async_resource_state_finished(job.state))
							{
								active_count += 1;
							}
							if (job.state == AsyncResourceJobState::Failed)
							{
								failed_count += 1;
							}
							visible_count += 1;
						}
						ImGui::Text("Jobs: %zu | Active: %zu | Failed: %zu", visible_count, active_count, failed_count);

						if (ImGui::BeginTable("##lstg.AsyncResourceJobs", 6,
							ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
						{
							ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
							ImGui::TableSetupColumn("Pool", ImGuiTableColumnFlags_WidthFixed);
							ImGui::TableSetupColumn("Resource");
							ImGui::TableSetupColumn("Files");
							ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
							ImGui::TableSetupColumn("Details");
							ImGui::TableHeadersRow();

							for (size_t i = 0; i < jobs.size(); ++i)
							{
								auto const& job = jobs[i];
								if (selected_pool_only && job.pool_type != selected_pool)
								{
									continue;
								}
								if (!show_finished && async_resource_state_finished(job.state))
								{
									continue;
								}

								std::string searchable = job.resource_name;
								for (auto const& file : job.files)
								{
									searchable.append("\n");
									searchable.append(file);
								}
								searchable.append("\n");
								searchable.append(job.error);
								if (!filter.PassFilter(searchable.c_str()))
								{
									continue;
								}

								ImGui::PushID(static_cast<int>(i));
								ImGui::TableNextRow();

								ImGui::TableSetColumnIndex(0);
								ImGui::TextUnformatted(async_resource_type_name(job));

								ImGui::TableSetColumnIndex(1);
								ImGui::TextUnformatted(async_resource_pool_name(job.pool_type));

								ImGui::TableSetColumnIndex(2);
								ImGui::TextUnformatted(job.resource_name.empty() ? "-" : job.resource_name.c_str());

								ImGui::TableSetColumnIndex(3);
								if (job.files.empty())
								{
									ImGui::TextDisabled("No file read");
								}
								else
								{
									for (auto const& file : job.files)
									{
										ImGui::TextWrapped("%s", file.c_str());
									}
								}

								ImGui::TableSetColumnIndex(4);
								ImGui::TextColored(async_resource_state_color(job.state), "%s", async_resource_state_name(job.state));

								ImGui::TableSetColumnIndex(5);
								if (!job.error.empty())
								{
									ImGui::TextWrapped("%s", job.error.c_str());
								}
								else if (job.state == AsyncResourceJobState::Ready)
								{
									ImGui::TextDisabled("Waiting for main thread");
								}

								ImGui::PopID();
							}
							ImGui::EndTable();
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Texture"))
					{
						static unsigned long long total_texture_memory_usage = 0;

						ImGui::Text("Total Resources: %u", p_pool->m_TexturePool.size());
						ImGui::Text("Total Adapter Memory Usage (Approximate): %s", bytes_count_to_string(total_texture_memory_usage).c_str());

						total_texture_memory_usage = 0;

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_TexturePool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								auto* p_res = v.second->GetTexture();
								auto const p_tex_size = p_res->getSize();
								unsigned long long mem_usage = p_tex_size.x * p_tex_size.y * 4;
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									static float preview_scale = 1.0f;
									draw_preview_scaling(preview_scale);
									draw_texture(*(v.second), true, preview_scale);
									ImGui::TreePop();
								}
								res_i += 1;
								total_texture_memory_usage += mem_usage;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Sprite"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_SpritePool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_SpritePool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									static float preview_scale = 1.0f;
									draw_preview_scaling(preview_scale);
									draw_sprite(v.second->GetSprite(), true, false, preview_scale);
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Sprite Sequence"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_AnimationPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_AnimationPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									static float preview_scale = 1.0f;
									draw_preview_scaling(preview_scale);
									ImGui::Text("Sprite Count: %u", v.second->GetCount());
									ImGui::Text("Animation Interval: %u", v.second->GetInterval());
									uint32_t ani_idx = v.second->GetSpriteIndexByTimer(timer);
									draw_sprite(v.second->GetSprite(ani_idx)->GetSprite(), false, false, preview_scale);
									static bool same_line = false;
									ImGui::Checkbox("Same Line Preview", &same_line);
									for (uint32_t img_idx = 0; img_idx < v.second->GetCount(); img_idx += 1)
									{
										if (same_line)
										{
											draw_sprite(v.second->GetSprite(img_idx)->GetSprite(), false, img_idx == ani_idx, preview_scale);
											if (img_idx < (v.second->GetCount() - 1))
												ImGui::SameLine();
										}
										else
										{
											if (ImGui::TreeNode(v.second->GetSprite(img_idx), "Sprite %u", img_idx))
											{
												draw_sprite(v.second->GetSprite(img_idx)->GetSprite(), true, false, preview_scale);
												ImGui::TreePop();
											}
										}
									}
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Music"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_MusicPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_MusicPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Sound Effect"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_SoundSpritePool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_SoundSpritePool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Particle System"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_ParticlePool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_ParticlePool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Sprite Font"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_SpriteFontPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_SpriteFontPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									auto* mgr = v.second->GetGlyphManager();
									auto* p_tex0 = mgr->getTexture(0);

									ImGui::Text("Size: %u x %u (x %u)", p_tex0->getSize().x, p_tex0->getSize().y, mgr->getTextureCount());
									ImGui::Text("Dynamic: No");
									unsigned long long mem_usage = p_tex0->getSize().x * p_tex0->getSize().y * 4 * mgr->getTextureCount();
									ImGui::Text("Adapter Memory Usage (Approximate): %s", bytes_count_to_string(mem_usage).c_str());

									static float preview_scale = 1.0f;
									draw_preview_scaling(preview_scale);
									for (uint32_t tidx = 0; tidx < mgr->getTextureCount(); tidx += 1)
									{
										draw_texture0(mgr->getTexture(tidx), preview_scale);
									}

									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Vector Font"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_TTFFontPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_TTFFontPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									auto* mgr = v.second->GetGlyphManager();
									auto* p_tex0 = mgr->getTexture(0);

									ImGui::Text("Size: %u x %u (x %u)", p_tex0->getSize().x, p_tex0->getSize().y, mgr->getTextureCount());
									ImGui::Text("Dynamic: Yes");
									unsigned long long mem_usage = p_tex0->getSize().x * p_tex0->getSize().y * 4 * mgr->getTextureCount();
									ImGui::Text("Memory Usage (Approximate): %s", bytes_count_to_string(mem_usage).c_str());
									ImGui::Text("Adapter Memory Usage (Approximate): %s", bytes_count_to_string(mem_usage).c_str());

									static float preview_scale = 1.0f;
									draw_preview_scaling(preview_scale);
									for (uint32_t tidx = 0; tidx < mgr->getTextureCount(); tidx += 1)
									{
										draw_texture0(mgr->getTexture(tidx), preview_scale);
									}

									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Post Effect"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_FXPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_FXPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Model"))
					{
						ImGui::Text("Total Resources: %u", p_pool->m_ModelPool.size());

						static ImGuiTextFilter filter;
						filter.Draw();

						int res_i = 0;
						for (auto& v : p_pool->m_ModelPool)
						{
							if (filter.PassFilter(v.second->GetResName().data()))
							{
								if (ImGui::TreeNode(*v.second,
									"%d. %s",
									res_i,
									v.second->GetResName().data()
								))
								{
									ImGui::TreePop();
								}
								res_i += 1;
							}
						}

						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}

			timer += 1;
		}
		ImGui::End();
	#endif
	}
}
