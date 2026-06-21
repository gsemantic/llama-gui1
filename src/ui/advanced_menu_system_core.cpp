#include "../include/ui/advanced_menu_system.h"
#include "../external/imgui/imgui.h"
#include "../include/ui/localization_manager.h"
// #include "../include/ui/agent_chat_integration.h"  // ОТКЛЮЧЕНО: агенты временно отключены
#include <iostream>
#include <functional>

namespace llama_gui {
namespace ui {

AdvancedMenu* AdvancedMenuSystem::findMenuInternal(const std::string& menu_name) {
    auto it = menus_map_.find(menu_name);
    return (it != menus_map_.end()) ? it->second : nullptr;
}

void AdvancedMenuSystem::initialize(CommandManager* command_manager, WindowManager* window_manager, WorkspaceManager* workspace_manager) {
    command_manager_ = command_manager;
    window_manager_ = window_manager;
    workspace_manager_ = workspace_manager;
    std::cout << "✓ AdvancedMenuSystem initialized" << std::endl;
}

void AdvancedMenuSystem::buildModernMenu() {
    std::cout << "Building modern menu structure..." << std::endl;

    // Создаем базовые меню в правильном порядке
    auto file_menu = createFileMenu();        // 1. Файл
    auto settings_menu = createSettingsMenu(); // 2. Настройки
    auto view_menu = createViewMenu();         // 3. Вид
    auto agents_menu = createAgentsMenu();     // 4. Агенты

    // Создаем меню для Developer workspace
    auto developer_menu = createDeveloperMenu(); // 5. Разработчик
    auto tools_menu = createToolsMenu();         // 6. Инструменты
    // Debug menu удалено - дублировало Developer menu

    // Создаем меню для Admin workspace
    auto performance_menu = createPerformanceMenu(); // 7. Производительность
    auto security_menu = createSecurityMenu();       // 8. Безопасность
    auto logging_menu = createLoggingMenu();         // 9. Логирование

    // Создаем меню Справка (последним)
    auto help_menu = createHelpMenu(); // 10. Справка (всегда последнее)

    // Добавляем базовые меню
    if (file_menu) addMenu(std::move(file_menu));
    if (settings_menu) addMenu(std::move(settings_menu));
    if (view_menu) addMenu(std::move(view_menu));
    if (agents_menu) addMenu(std::move(agents_menu));

    // Добавляем меню для разработчика
    if (developer_menu) addMenu(std::move(developer_menu));
    if (tools_menu) addMenu(std::move(tools_menu));
    // Debug menu удалено - дублировало Developer menu

    // Добавляем меню для администратора
    if (performance_menu) addMenu(std::move(performance_menu));
    if (security_menu) addMenu(std::move(security_menu));
    if (logging_menu) addMenu(std::move(logging_menu));

    // Добавляем меню Справка (всегда последнее)
    if (help_menu) addMenu(std::move(help_menu));

    // Добавляем callback для обновления меню при смене workspace
    if (workspace_manager_) {
        workspace_manager_->addWorkspaceChangedCallback([this](WorkspaceType type) {
            updateMenuVisibilityForWorkspace(type);
        });
    }

    std::cout << "✓ Modern menu built (" << menus_ordered_.size() << " menus)" << std::endl;
}

void AdvancedMenuSystem::rebuildModernMenu() {
    std::cout << "Rebuilding menu for language change..." << std::endl;

    // Сохраняем текущие состояния workspace
    WorkspaceType current_workspace = WorkspaceType::User;
    if (workspace_manager_) {
        current_workspace = workspace_manager_->getCurrentWorkspaceType();
    } else {
        std::cerr << "Warning: workspace_manager_ is null, using default User workspace" << std::endl;
    }

    // Очищаем все меню
    menus_ordered_.clear();
    menus_map_.clear();

    // Перестраиваем меню с новыми переводами
    buildModernMenu();

    // Восстанавливаем видимость меню для текущего workspace
    updateMenuVisibilityForWorkspace(current_workspace);

    std::cout << "✓ Menu rebuilt for language change" << std::endl;
}

bool AdvancedMenuSystem::addMenu(std::unique_ptr<AdvancedMenu> menu) {
    if (!menu) {
        std::cerr << "Cannot add null menu" << std::endl;
        return false;
    }

    std::string menu_name = menu->name;
    menus_map_[menu_name] = menu.get();
    menus_ordered_.push_back(std::move(menu));
    return true;
}

bool AdvancedMenuSystem::addMenu(const std::string& name, std::vector<AdvancedMenuItem> items) {
    auto menu = std::make_unique<AdvancedMenu>();
    menu->name = name;
    menu->items = std::move(items);
    return addMenu(std::move(menu));
}

bool AdvancedMenuSystem::addMenuItem(const std::string& menu_name, const AdvancedMenuItem& item) {
    auto it = menus_map_.find(menu_name);
    if (it == menus_map_.end()) {
        std::cerr << "Menu not found: " << menu_name << std::endl;
        return false;
    }

    it->second->items.push_back(item);
    std::cout << "✓ Added menu item: " << item.name << " to menu: " << menu_name << std::endl;
    return true;
}

bool AdvancedMenuSystem::addSeparator(const std::string& menu_name) {
    AdvancedMenuItem separator;
    separator.type = AdvancedMenuItemType::Separator;
    return addMenuItem(menu_name, separator);
}

bool AdvancedMenuSystem::removeMenu(const std::string& menu_name) {
    auto it = menus_map_.find(menu_name);
    if (it == menus_map_.end()) {
        return false;
    }

    // Удаляем из map
    menus_map_.erase(it);
    
    // Удаляем из ordered списка
    auto ordered_it = std::find_if(menus_ordered_.begin(), menus_ordered_.end(),
        [&menu_name](const std::unique_ptr<AdvancedMenu>& menu) {
            return menu->name == menu_name;
        });
    
    if (ordered_it != menus_ordered_.end()) {
        menus_ordered_.erase(ordered_it);
    }
    
    std::cout << "✓ Removed menu: " << menu_name << std::endl;
    return true;
}

bool AdvancedMenuSystem::removeMenuItem(const std::string& menu_name, const std::string& item_name) {
    auto it = menus_map_.find(menu_name);
    if (it == menus_map_.end()) {
        return false;
    }

    auto& items = it->second->items;
    auto item_it = std::find_if(items.begin(), items.end(),
        [&item_name](const AdvancedMenuItem& item) {
            return item.name == item_name;
        });

    if (item_it == items.end()) {
        return false;
    }

    items.erase(item_it);
    std::cout << "✓ Removed menu item: " << item_name << " from menu: " << menu_name << std::endl;
    return true;
}

void AdvancedMenuSystem::updateMenuStates() {
    // Update menu states based on current application state
    for (auto& menu : menus_ordered_) {
        for (auto& item : menu->items) {
            // Skip if no window manager
            if (!window_manager_) continue;

            // Update window toggle items
            if (item.is_window_toggle && !item.window_name.empty()) {
                item.checked = window_manager_->isWindowVisible(item.window_name);
            }
        }
    }
}

void AdvancedMenuSystem::setMenuItemEnabled(const std::string& menu_name, const std::string& item_name, bool enabled) {
    auto* menu = findMenuInternal(menu_name);
    if (menu) {
        for (auto& item : menu->items) {
            if (item.name == item_name) {
                item.enabled = enabled;
                return;
            }
        }
    }
}

void AdvancedMenuSystem::setMenuEnabled(const std::string& menu_name, bool enabled) {
    auto* menu = findMenuInternal(menu_name);
    if (menu) {
        menu->enabled = enabled;
    }
}

void AdvancedMenuSystem::setMenuItemChecked(const std::string& menu_name, const std::string& item_name, bool checked) {
    auto* menu = findMenuInternal(menu_name);
    if (menu) {
        for (auto& item : menu->items) {
            if (item.name == item_name) {
                item.checked = checked;
                return;
            }
        }
    }
}

bool AdvancedMenuSystem::isMenuItemChecked(const std::string& menu_name, const std::string& item_name) const {
    auto* menu = const_cast<AdvancedMenuSystem*>(this)->findMenuInternal(menu_name);
    if (menu) {
        for (const auto& item : menu->items) {
            if (item.name == item_name) {
                return item.checked;
            }
        }
    }
    return false;
}

// Фабричные методы создания меню перенесены в advanced_menu_system_factories.cpp

namespace AdvancedMenuItemFactory {

AdvancedMenuItem createCommandItem(const std::string& name, const std::string& command,
                                  const std::string& shortcut, const std::string& tooltip) {
    AdvancedMenuItem item;
    item.name = name;
    item.command = command;
    item.shortcut = shortcut;
    item.tooltip = tooltip;
    item.enabled = true;
    item.type = AdvancedMenuItemType::Item;
    return item;
}

AdvancedMenuItem createSeparator() {
    AdvancedMenuItem item;
    item.name = "";
    item.type = AdvancedMenuItemType::Separator;
    item.separator = true;
    return item;
}

AdvancedMenuItem createCheckboxItem(const std::string& name, const std::string& command,
                                   std::function<bool()> check_func, std::function<void(bool)> toggle_func,
                                   const std::string& shortcut, const std::string& tooltip) {
    AdvancedMenuItem item = createCommandItem(name, command, shortcut, tooltip);
    item.check_func = std::move(check_func);
    item.toggle_func = std::move(toggle_func);
    return item;
}

AdvancedMenuItem createWindowToggleItem(const std::string& name, const std::string& window_name,
                                       const std::string& shortcut, const std::string& tooltip) {
    AdvancedMenuItem item = createCommandItem(name, "toggle_window_" + window_name, shortcut, tooltip);
    item.window_name = window_name;
    item.is_window_toggle = true;
    return item;
}

} // namespace AdvancedMenuItemFactory

// =========================================================================
// Workspace Manager методы
// =========================================================================

void AdvancedMenuSystem::setWorkspaceManager(WorkspaceManager* workspace_manager) {
    workspace_manager_ = workspace_manager;
}

void AdvancedMenuSystem::switchToWorkspace(WorkspaceType type) {
    std::cout << "[MENU] switchToWorkspace called: " << workspace_type_to_string(type) << std::endl;
    if (workspace_manager_) {
        workspace_manager_->switchWorkspace(type);
        std::cout << "[MENU] switchToWorkspace completed" << std::endl;
    }
}

void AdvancedMenuSystem::switchToWorkspace(const std::string& name) {
    std::cout << "[MENU] switchToWorkspace(string) called: " << name << std::endl;
    if (workspace_manager_) {
        workspace_manager_->switchWorkspace(name);
        std::cout << "[MENU] switchToWorkspace completed" << std::endl;
    }
}

void AdvancedMenuSystem::updateMenuVisibilityForWorkspace(WorkspaceType type) {
    if (!workspace_manager_) return;

    const auto& config = workspace_manager_->getCurrentWorkspace();
    std::cout << "[MENU] updateMenuVisibilityForWorkspace: " << config.name << std::endl;

    // Скрываем/показываем меню в зависимости от workspace
    for (auto& menu : menus_ordered_) {
        // Меню Workspace и Window всегда видимы для переключения
        if (menu->menu_key == "Workspace" || menu->menu_key == "Window") {
            menu->enabled = true;
            continue;
        }

        bool visible = workspace_manager_->isMenuVisible(menu->menu_key);
        std::cout << "[MENU]   '" << menu->name << "' (key=" << menu->menu_key << ") -> " << (visible ? "VISIBLE" : "HIDDEN") << std::endl;
        menu->enabled = visible;
    }
}

// Фабричные методы создания меню перенесены в advanced_menu_system_factories.cpp

} // namespace ui
} // namespace llama_gui
