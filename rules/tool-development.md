# tool-development -- MCP Tool Addition Procedure and Phase-by-Phase Tool List

## 4-Step Process to Add New Tool

1. **Python Side**: Add Tool function to `tools/` directory (decorator, type hints, docstring)
2. **C++ Side**: Add command handler function to corresponding Handler
3. **Testing**: Write minimum 1 success case + 1 error case, perform UE5 editor integration test
4. **Documentation**: Add Tool specification to `docs/tools-reference.md`

---

## Phase 1: Actor & Scene (Basic)
| Tool | Description |
|------|-------------|
| `create_actor` | Create actor (StaticMesh, Light, Camera, etc) |
| `delete_actor` | Delete actor |
| `set_actor_transform` | Set location/rotation/scale |
| `get_actors_in_level` | Query actor list in level |
| `find_actors_by_name` | Search actors by name |
| `duplicate_actor` | Duplicate actor |
| `get_actor_properties` | Read actor properties |
| `set_actor_property` | Write actor property |

## Phase 2: Blueprint Editing
| Tool | Description |
|------|-------------|
| `create_blueprint` | Create new Blueprint class |
| `add_blueprint_node` | Add node to BP graph |
| `connect_blueprint_pins` | Connect node pins |
| `remove_blueprint_node` | Remove node |
| `add_blueprint_variable` | Add variable |
| `compile_blueprint` | Compile Blueprint |
| `get_blueprint_graph` | Read graph structure |
| `add_blueprint_component` | Add component |
| `spawn_blueprint_actor` | Spawn actor from BP |

## Phase 3: Material & Asset
| Tool | Description |
|------|-------------|
| `search_assets` | Search assets (name/type filter) |
| `get_asset_details` | Get asset details |
| `create_material` | Create material |
| `add_material_expression` | Add material node |
| `connect_material_nodes` | Connect material nodes |
| `apply_material_to_actor` | Apply material to actor |
| `set_material_parameter` | Set material instance parameter |
| `import_asset` | Import external asset |
| `duplicate_asset` | Duplicate asset |
| `delete_asset` | Delete asset |

## Phase 4: AI System
| Tool | Description |
|------|-------------|
| `create_behavior_tree` | Create Behavior Tree asset |
| `add_bt_node` | Add Task/Decorator/Service node to BT |
| `create_blackboard` | Create Blackboard asset |
| `add_blackboard_key` | Add Blackboard key |
| `create_eqs_query` | Create EQS query |
| `setup_ai_perception` | Configure AIPerception component |
| `create_ai_controller` | Create AIController Blueprint |

## Phase 5: Editor Automation
| Tool | Description |
|------|-------------|
| `play_in_editor` | Start/stop PIE |
| `set_viewport_camera` | Set viewport camera position/direction |
| `run_console_command` | Execute console command |
| `take_screenshot` | Take viewport screenshot |
| `get_selected_actors` | Query currently selected actors |
| `select_actors` | Select actors |
| `save_level` | Save level |
| `load_level` | Load level |

## Phase 6: Advanced Systems (Extensions)
| Tool | Description |
|------|-------------|
| `create_niagara_system` | Create Niagara particle system |
| `create_animation_blueprint` | Create animation BP |
| `create_widget_blueprint` | Create UMG widget BP |
| `create_data_table` | Create DataTable asset |
| `create_data_asset` | Create DataAsset |
| `inspect_uobject` | Query arbitrary UObject properties |

---

## Testing Principles
- All Tools must have minimum **1 success + 1 error** test
- Blueprint Tools: test full flow **create -> modify -> compile -> spawn**
- Verify appropriate error message returned when UE5 editor not running
