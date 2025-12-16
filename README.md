# 基于逐飞TC264开源库的21届智能车代码（修改中）🛵💨
## ⚠️ 郑重警告：完成git clone后立刻解压 `libraries/infineon_libraries.zip`
## ⚠️ 郑重警告：请务必关闭WindowsDefender等杀毒软件对工程目录的实时保护，否则可能导致编译失败

# 分支说明
- `master`：主分支 包含PID/LQR等功能的完整代码（等待首版发布）
- `PTC-noPID`：基于逐飞MT9V034总钻风摄像头的光电循迹验证代码 无PID控制（正在开发）
- `PTC`：基于逐飞MT9V034总钻风摄像头的光电循迹验证代码 含PID/LQR控制算法（待开发）

# 小贴士
- 使用中断需要在 `user/isr.c`注释对应片段

# 许可证与版权
> TC264 Opensource Library（TC264 开源库）基于官方 SDK 接口的第三方开源库。
> Copyright (c) 2022 SEEKFREE 逐飞科技
>
> 本文件是 TC264 开源库的一部分，遵循 GPL 第 3 版或任意更新版本。
> 发布目的是方便使用，但不提供任何明示或暗示的保证，适销性和特定用途适用性均不担保。详情参见 GPL。
> 如未获随附 GPL 副本，请访问 <https://www.gnu.org/licenses/>。
>
> 额外说明：许可申明译文如上，英文原文在 `libraries/doc/GPL3_permission_statement.txt`，许可证副本在 `libraries/LICENSE`。使用与传播请保留逐飞科技版权声明。
>
> 公司名称：成都逐飞科技有限公司
> 版本信息：见 `libraries/doc/version`
> 开发环境：ADS v1.10.2 or later
> 适用平台：Seekfree TC264D
