# Seekfree_TC264_ZBY 🤹‍♂️

基于逐飞TC264开源库的21届智能车代码（修改中）🛵💨
⚠️ 正经提醒：从 GitHub 拉完代码，立刻解压 `libraries/infineon_libraries.zip`，否则编译会当场翻脸报错

## 上电即跑，改一点点就能飞。🚀

## 目录速览
- `code/` 与 `user/`：用户代码主战场，`cpu0_main.c`、`isr.c` 等入口都在这
- `libraries/`：英飞凌 iLLD、Seekfree 公共库与设备驱动，别删，删了就“无驱可用”
- `Lcf_Tasking_Tricore_Tc.lsl`：链接脚本，轻拿轻放

## 小贴士
- 想快速定位主循环？看 `user/cpu0_main.c`，使用中断需要在 `user/isr.c`注释对应片段，不然报错到姥姥家

祝你玩得开心，少踩坑，多上电成功！🎉
