---
name: ue5-fps-project
description: Use when working on the Unreal Engine multiplayer FPS project at F:\Temp\FPS — connecting through the in-editor Unreal MCP, reading Blueprint/Animation Blueprint logic, the C++ class map, and the animation data flow. Covers MCP session recovery, tool-search workflow, and the project's real architecture.
---

# UE5 FPS 项目工作手册

工程：`F:\Temp\FPS`，UE **5.8.4**（安装版），模块 `FPS`，目标做多人 FPS。

## 一、Unreal MCP 接入

编辑器内置插件 **ModelContextProtocol**（友好名 Unreal MCP）+ **All Toolsets**，监听
`http://127.0.0.1:8000/mcp`（streamable-http，无认证，仅回环）。

DSH 侧配置：
- 插件 `@deepseek-ai/dsh-mcp-client`（装在 web profile，版本须与 dsh 一致）
- 条目 `mcp-unreal` 在 `C:\Users\Kevin Yang\.dsh\profiles\web\cordis.patch.yml`
- 工具名形如 `mcp__unreal__call_tool`

**调用顺序**：`list_toolsets` → `describe_toolset` → `call_tool`（tool-search 模式默认开启，`tools/list` 只暴露 3 个元工具）。

### 会话失效（高频）

编辑器重启或启停 toolset 后，调用会报：

```
Unknown session id '...' for 'tools/call'; client should reinitialize
```

原因是客户端持有旧会话，而服务端回的是 JSON-RPC 错误（不是传输断开），插件不会自动重建。

修复任选其一：
1. 编辑器控制台：`ModelContextProtocol.StopServer` 然后 `ModelContextProtocol.StartServer`（推荐，关闭会话促使客户端重连）
2. 改 `cordis.patch.yml` 里 `mcp-unreal` 的任一配置值触发热重载
3. 重启 `dsh web`

改配置后**要等几秒**，重连完成前调用仍会失败。

### 调用约束

- Tool 调用在**游戏线程串行执行**，不要并发调用。
- 新增 `UFUNCTION` 需要重启编辑器；Live Coding 只传播已有函数体的修改。

## 二、C++ 类地图（实测读取）

| 类 | 关键成员 |
|---|---|
| `AShooterCharacter` | `combat`(UCombatComponent)、`mesh1P`、`springArm`、`firstPersonCamera`、`defaultFieldOfView`、4 个 Enhanced Input Action（CycleWeapon / FireWeapon / ReloadWeapon / AimWeapon） |
| `UCombatComponent` | `weaponData`(UWeaponData)、`bAiming`、`currentWeapon`(AWeapon)、`defaultWeaponClasses` |
| `UWeaponData` | `gripPoints`、`firstPersonAnims`、`thirdPersonAnims`（按 GameplayTag 索引的动画组） |
| `FPlayerAnims` | `idleAnim`/`aimIdleAnim`/`crouchIdleAnim`/`sprintAnim`(AnimSequence) + `aimOffset_Hip`/`aimOffset_Aim`/`strafe_Standing`/`strafe_Crouching`(BlendSpace) |
| `AWeapon` | `WeaponType`(FGameplayTag)、`Mesh1P`/`Mesh3P`、`AttachToOwningPawn()` |
| `UPlayerInterface` | `GetWeaponAttachPoint(Tag)`、`GetMesh1P()`、`GetMesh3P()` |

蓝图资产都在 `Content/FPS/`：`BP_ShooterCharacter`、`BP_Weapon`/`BP_Pistol`/`BP_Rifle`、`BP_PlayerController`、`BP_ShooterGameMode`、`ABP_FirstPerson`、`ABP_ThirdPerson`、`IMC_Shooter`、`IA_*`、`DA_WeaponData`。

构建配置：`Source/*.Target.cs` 用 `BuildSettingsVersion.V7` + `EngineIncludeOrderVersion.Unreal5_8`（V6 会因与 UnrealEditor 共享编译产物而被 UBT 拒绝）。

## 三、动画数据流（ABP_ThirdPerson，已实测）

四个图：`AnimGraph`、`GetCurrentAnims`（函数）、`BlueprintThreadSafeUpdateAnimation`、`EventGraph`。

变量：`As Shooter Character`(AShooterCharacter)、`equippedWeaponType`(FGameplayTag)、`currentAnims`(FPlayerAnims)、`isAiming`(bool)。

实际逻辑（DSL 原文）：

```lisp
(event EventBlueprintInitializeAnimation
  (bind _asshooter_character (Utilities|Casting|CastToShooterCharacter (Animation|TryGetPawnOwner)))
  (Variables|Default|SetAsShooterCharacter _asshooter_character))

(fn GetCurrentAnims ()
  (Variables|Default|SetEquippedWeaponType (Variables|PropertyAccess))
  (Variables|Default|SetCurrentAnims (Utilities|Map|Find (Variables|PropertyAccess) (GetEquippedWeaponType))))

(fn BlueprintThreadSafeUpdateAnimation (DeltaTime)
  (Variables|Default|GetCurrentAnims)
  (Variables|Default|SetIsAiming (Variables|PropertyAccess)))
```

语义：`Character.combat.currentWeapon.weaponData.thirdPersonAnims[equippedWeaponType] → currentAnims`；
`combat.bAiming → isAiming`。

**结论**：这个 ABP 是薄壳，数据全在 C++（`UWeaponData` / `UCombatComponent`），转 C++ 收益极小。

## 四、关键限制

- **AnimGraph 读不到**：`read_graph_dsl` 对 AnimGraph 返回空字符串（DSL 只覆盖 K2 节点，动画图节点不在内）。状态机 / BlendSpace 播放器 / Layered Blend 必须留在资产里。
- **线程安全**：`NativeThreadSafeUpdateAnimation` 跑在动画工作线程，不要在里面解引用 UObject 链（`->GetCombat()->GetWeaponData()` 之类）。要么放 `NativeUpdateAnimation`（游戏线程），要么在 `FAnimInstanceProxy::Update` 里用预拷贝数据。
- **同名变量遮蔽**：ABP reparent 到 C++ 基类后，蓝图里同名旧变量必须删除，否则动画图读的是资产里那个没人更新的变量。
- **`Content/*` 被 .gitignore 排除**，只有 `Content/FPS/**` 进版本库；模板资源无版本保护，升级引擎前必须整目录备份。

## 五、常用工具速查

| 目的 | toolset.tool |
|---|---|
| 找资产 | `AssetTools.find_assets` / `exists` / `get_dependencies` |
| 读蓝图图 | `BlueprintTools.list_graphs` → `read_graph_dsl` |
| 读变量 | `ObjectTools.list_properties`（传类的 refPath 可读 C++ 类） |
| 读属性值 | `ObjectTools.get_properties` |
| 编译 | `LiveCodingToolset` |
| 读日志 | `EditorToolset.LogsToolset` |
| 放 Actor | `SceneTools` / `ActorTools` |
| 材质 | `MaterialTools` / `MaterialInstanceTools` |

`refPath` 格式：资产实例 `/Game/Path/Asset.Asset`，生成类 `/Game/Path/ASSET.ASSET_C`，C++ 类 `/Script/Module.ClassName`。
