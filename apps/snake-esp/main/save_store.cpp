// SPDX-License-Identifier: GPL-3.0-or-later
#include "save_store.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::esp
{

namespace
{

constexpr const char* TAG = "snake";
constexpr const char* NAMESPACE = "snake";
constexpr const char* KEY = "save";

} // namespace

SaveStore::SaveStore()
{
    // Not erased when it fails (ESP_ERR_NVS_NO_FREE_PAGES, a newer version): it may not be Snake's to erase.
    const esp_err_t error = nvs_flash_init();
    m_Ready = error == ESP_OK;
    if (!m_Ready)
    {
        ESP_LOGW(TAG, "the storage did not open, so nothing is saved: %s", esp_err_to_name(error));
    }
}

snake::profile::SaveData SaveStore::load()
{
    if (!m_Ready)
    {
        return {};
    }
    nvs_handle_t handle = 0;
    if (nvs_open(NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return {}; // nothing was saved yet
    }
    std::array<std::uint8_t, snake::profile::MAX_ENCODED_SAVE_SIZE> bytes{};
    std::size_t size = bytes.size();
    const esp_err_t error = nvs_get_blob(handle, KEY, bytes.data(), &size);
    nvs_close(handle);
    if (error != ESP_OK)
    {
        return {};
    }
    const auto decoded = snake::profile::decode_save(std::span<const std::uint8_t>(bytes.data(), size));
    if (!decoded)
    {
        m_Writable = decoded.error() != snake::profile::SaveError::NEWER_VERSION;
        ESP_LOGW(TAG, "the saved profile was not used (error %u)", static_cast<unsigned>(decoded.error()));
        return {};
    }
    return *decoded;
}

void SaveStore::store(const snake::profile::SaveData& data)
{
    if (!m_Ready || !m_Writable)
    {
        return;
    }
    std::array<std::uint8_t, snake::profile::MAX_ENCODED_SAVE_SIZE> bytes{};
    const std::size_t size = snake::profile::encode_save(data, bytes);
    if (size == 0)
    {
        return;
    }
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (error == ESP_OK)
    {
        error = nvs_set_blob(handle, KEY, bytes.data(), size);
        if (error == ESP_OK)
        {
            error = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (error != ESP_OK)
    {
        ESP_LOGW(TAG, "the profile was not saved: %s", esp_err_to_name(error));
    }
}

} // namespace snake::esp
