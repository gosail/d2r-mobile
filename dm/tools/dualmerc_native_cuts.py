"""Switch 3.3 dual-merc native call-site audit cuts (experimental only).

Three existing BL calls are replaced with host SVC pass-through gateways, not
removed: all three native functions still execute in their original order.
No other game build is supported. The NSO patcher independently checks the
exact original bytes before applying each cut.
"""
import struct

BUILD_ID = "db0507f36b84200ac05f75d76ed5026f"
# site RVA, original callee RVA, host svc, message
SITES = (
    (0x6877F0, 0x6C0C30, 0x91, "old pet deregistration candidate"),
    (0x68786C, 0x7CB6D0, 0x92, "old unit teardown candidate"),
    (0x687940, 0x6C1B00, 0x93, "new pet binding candidate"),
)


def branch_word(site, dest):
    d = dest - site
    if d % 4 or d < -(1 << 27) or d >= (1 << 27):
        raise ValueError("invalid AArch64 BL displacement")
    return 0x94000000 | ((d // 4) & 0x03ffffff)


def svc_word(num):
    if not 0x80 <= num <= 0xff:
        raise ValueError("host SVC number out of range")
    return 0xd4000001 | (num << 5)


def host_audit_cuts(build_id):
    if build_id != BUILD_ID:
        raise ValueError("unverified Switch main Build ID")
    return [(site, struct.pack("<I", branch_word(site, callee)).hex(),
             struct.pack("<I", svc_word(svc)).hex(),
             "双佣兵实验原生调用观测："
             + why + "，svc 0x%x 原样转调目标 main+0x%x；只有离线实验资源构建启用" % (svc, callee),
             "仅宿主")
            for site, callee, svc, why in SITES]
