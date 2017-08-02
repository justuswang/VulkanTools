
#include "override_settings.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

// This is only needed to keep the manifest version up to date. Other than that, it shouldn't be needed
#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#define WIN_BUFFER_SIZE 1024
#endif

OverrideSettings::OverrideSettings()
{
    // Load the override layer (if found)
    QFile layer_file(LayerFile(false));
    if (layer_file.exists() && layer_file.open(QFile::ReadOnly)) {
        QString data = layer_file.readAll();
        layer_file.close();
        
        QJsonDocument document = QJsonDocument::fromJson(data.toLocal8Bit());
        if (document.isObject()) {
            QJsonValue layer = document.object().value("layer");
            if (layer.isObject()) {
                QJsonValue components = layer.toObject().value("component_layers");
                if (components.isArray()) {
                    for(const QJsonValue& component : components.toArray()) {
                        if (!component.isString()) {
                            continue;
                        }
                        layers.append(component.toString());
                    }
                }
            }
        }
    }
    
    // Load the settings file (if found)
    QFile settings_file(LayerSettingsFile(false));
    if (settings_file.exists() && settings_file.open(QFile::ReadOnly)) {
        QString data = settings_file.readAll();
        settings_file.close();
        
        for (const QString& line : data.split(QRegularExpression("\n|\r\n|\r"))) {
            if (QRegularExpression("\\s*#").match(line).hasMatch()) {
                continue;
            }
            
            QList<QString> sections = line.split(".");
            if (sections.size() != 2) {
                continue;
            }
            QString layer_abbrev = sections[0];
            QList<QString> subsections = sections[1].split("=");
            if (subsections.size() != 2) {
                continue;
            }
            QString setting_name = subsections[0];
            QString setting_value = subsections[1];
            
            QList<QString> layer_sections = layer_abbrev.split("_");
            layer_sections[0].toUpper();
            layer_sections.prepend("LAYER");
            layer_sections.prepend("VK");
            QString layer_name = layer_sections.join("_");
            
            layer_settings[layer_name][setting_name] = setting_value;
        }
    }
}

QList<QString> OverrideSettings::Layers() const
{
    return layers;
}

QHash<QString, QHash<QString, QString>> OverrideSettings::LayerSettings() const
{
    return layer_settings;
}

bool OverrideSettings::SaveLayers(const QList<QPair<QString, LayerType>> &paths, const QList<LayerManifest> &layers, int expiration)
{
    QDateTime now = QDateTime::currentDateTime();
    now = now.addSecs(expiration);
        
    QJsonArray json_paths;
    for (const QPair<QString, LayerType> &pair : paths) {
        json_paths.append(pair.first);
    }
    QJsonArray json_layers;
    for (const LayerManifest &manifest : layers) {
        json_layers.append(manifest.name);
    }
    QJsonObject disable;
    disable.insert("DISABLE_VK_LAYER_LUNARG_override", "1");
    
    QJsonObject layer;
    layer.insert("name", "VK_LAYER_LUNARG_override");
    layer.insert("type", "GLOBAL");
    layer.insert("api_version", "1.0." + QString::number(VK_HEADER_VERSION));
    layer.insert("implementation_version", "1");
    layer.insert("description", "LunarG Override Layer");
    if (expiration >= 0) {
        layer.insert("expiration_date", now.toString("yyyy-MM-dd-hh-mm"));
    }
    layer.insert("override_paths", json_paths);
    layer.insert("component_layers", json_layers);
    layer.insert("disable_environment", disable);
    
    QJsonObject root;
    root.insert("file_format_version", QJsonValue("1.1.2"));
    root.insert("layer", layer);
    QJsonDocument doc(root);
    
    QFile layer_file(LayerFile(true));
    if (!layer_file.open(QFile::WriteOnly)) {
        return true;
    }
    layer_file.write(doc.toJson());
    layer_file.close();
    
    return false;
}

bool OverrideSettings::SaveSettings(const QHash<QString, QHash<QString, LayerValue>> &settings)
{
    QFile settings_file(LayerSettingsFile(true));
    if (!settings_file.open(QFile::WriteOnly)) {
        return true;
    }
    
    QTextStream stream(&settings_file);
    for (const QString &layer : settings.keys()) {
        if (!layer.startsWith("VK_LAYER_")) {
            continue;
        }
        QString short_layer = layer.right(layer.size() - 9).toLower();
        
        stream << "\n# " << layer << "\n";
        for (const QString &option : settings[layer].keys()) {
            QString string_value = settings[layer][option].values.values().join(",");
            stream << short_layer << "." << option << " = " << string_value << "\n";
        }
    }
    settings_file.close();

    return false;
}

QString OverrideSettings::LayerFile(bool create_path) const
{
#if defined(_WIN32)
    // Open or create the implicit layer registry key
    HKEY key;
    if (create_path) {
        LSTATUS err = RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers", 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &key, NULL);
        if (err != VK_SUCCESS) {
            return "";
        }
    } else {
        LSTATUS err = RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers", NULL, KEY_READ, &key);
        if (err != ERROR_SUCCESS) {
            return "";
        }
    }

    // Read the filename into path
    QString path;
    DWORD value_count;
    RegQueryInfoKey(key, NULL, NULL, NULL, NULL, NULL, NULL, &value_count, NULL, NULL, NULL, NULL);
    for (DWORD i = 0; i < value_count; ++i) {
        TCHAR file_path[WIN_BUFFER_SIZE];
        DWORD buff_size = WIN_BUFFER_SIZE;
        RegEnumValue(key, i, file_path, &buff_size, NULL, NULL, NULL, NULL);

        path = file_path;
    }

    if(create_path && path.isEmpty()) {
        // Find a good location for the file
        QString data_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (data_path.isEmpty()) {
            RegCloseKey(key);
            return "";
        }

        QDir data_dir(data_path);
        data_dir.mkpath(".");
        path = data_dir.absoluteFilePath("VkLayer_override.json");
        QByteArray path_bytes = path.toLocal8Bit();

        // Write the location to the registry key
        DWORD reg_value = 0;
        LSTATUS err = RegSetValueEx(key, path_bytes.data(), 0, REG_DWORD, (BYTE*) &reg_value, NULL);
        if (err != VK_SUCCESS) {
            RegCloseKey(key);
            return "";
        }
    }

    // Close the key and exit
    RegCloseKey(key);
    return path;
#else
    QDir implicit_path(QDir::homePath() + "/.local/share/vulkan/implicit_layer.d")
    if (create_path) {
        implicit_path.mkpath(".");
    }
    return implicit_path.absoluteFilePath("VkLayer_override.json");
#endif
}

QString OverrideSettings::LayerSettingsFile(bool create_path) const
{
#if defined(_WIN32)
    // Open or create the settings registry key
    HKEY key;
    if (create_path) {
        LSTATUS err = RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Khronos\\Vulkan\\Settings", 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &key, NULL);
        if (err != VK_SUCCESS) {
            return "";
        }
    } else {
        LSTATUS err = RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Khronos\\Vulkan\\Settings", NULL, KEY_READ, &key);
        if (err != ERROR_SUCCESS) {
            return "";
        }
    }

    // Read the filename into path
    QString path;
    DWORD value_count;
    RegQueryInfoKey(key, NULL, NULL, NULL, NULL, NULL, NULL, &value_count, NULL, NULL, NULL, NULL);
    for (DWORD i = 0; i < value_count; ++i) {
        TCHAR file_path[WIN_BUFFER_SIZE];
        DWORD buff_size = WIN_BUFFER_SIZE;
        RegEnumValue(key, i, file_path, &buff_size, NULL, NULL, NULL, NULL);

        path = file_path;
    }

    if(create_path && path.isEmpty()) {
        // Find a good location for the file
        QString data_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (data_path.isEmpty()) {
            RegCloseKey(key);
            return "";
        }

        QDir data_dir(data_path);
        data_dir.mkpath(".");
        path = data_dir.absoluteFilePath("vk_layer_settings.txt");
        QByteArray path_bytes = path.toLocal8Bit();

        // Write the location to the registry key
        DWORD reg_value = 0;
        LSTATUS err = RegSetValueEx(key, path_bytes.data(), 0, REG_DWORD, (BYTE*) &reg_value, NULL);
        if (err != VK_SUCCESS) {
            RegCloseKey(key);
            return "";
        }
    }

    // Close the key and exit
    RegCloseKey(key);
    return path;
#else
    QDir settings_path(QDir::homePath() + "/.local/share/vulkan/settings.d")
    if (create_path) {
        settings_path.mkpath(".");
    }
    return settings_path.absoluteFilePath("vk_layer_settings.txt");
#endif    
}
