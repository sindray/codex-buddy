#include "buddy_state.h"
#include <string.h>

const char *const buddy_ids[B_COUNT] = {
    "boot", "connecting", "idle", "received", "thinking", "planning", "reading",
    "searching", "coding", "building", "testing", "reviewing", "waiting", "approval",
    "done", "error", "cancelled", "reconnecting", "sleeping", "waving"
};
const char *const buddy_titles[B_COUNT] = {
    "开机啦", "正在连接", "待命", "收到任务", "思考中", "安排一下", "阅读代码",
    "搜索中", "正在工作", "构建中", "测试中", "检查代码", "等待你", "等待审批",
    "已完成", "遇到问题", "已取消", "连接断开", "打个盹", "你好呀"
};
const char *const buddy_subtitles[B_COUNT] = {
    "今天也一起写点东西", "等待电脑桥接", "等你安排任务", "准备开始干活", "正在整理思路",
    "把任务拆成小块", "先把代码看明白", "正在寻找线索", "正在处理任务", "正在打包代码",
    "看看能不能通过", "再仔细检查一遍", "请到电脑回答", "请到电脑确认", "任务已结束",
    "请查看电脑详情", "工具先收起来", "等待重新连接", "按键就能唤醒", "继续一起加油"
};
buddy_state_t buddy_parse_state(const char *id) {
    if (id) for (int i = 0; i < B_COUNT; ++i)
        if (!strcmp(id, buddy_ids[i])) return (buddy_state_t)i;
    return B_COUNT;
}
void buddy_init(buddy_model_t *m, uint64_t now) {
    *m = (buddy_model_t){.remote=B_IDLE, .changed_at=now, .touched_at=now};
}
void buddy_receive(buddy_model_t *m, buddy_state_t state, uint64_t now, bool new_task) {
    if (state < 0 || state >= B_COUNT) return;
    if (!m->connected || state != m->remote || new_task) m->changed_at=now;
    if (new_task || state != m->remote) m->touched_at=now;
    m->remote=state; m->last_packet=now; m->connected=true; m->ever_connected=true;
}
void buddy_touch(buddy_model_t *m, uint64_t now) { m->touched_at=now; }
buddy_state_t buddy_visible(const buddy_model_t *m, uint64_t now) {
    if (!m->connected) return now < 2000 ? B_BOOT : B_CONNECT;
    if (now-m->last_packet > 15000) return B_RECONNECT;
    if (m->remote == B_IDLE || m->remote == B_DONE || m->remote == B_CANCEL) {
        uint64_t idle_since = m->changed_at > m->touched_at ? m->changed_at : m->touched_at;
        if (now-idle_since > 90000) return B_SLEEP;
        if ((m->remote == B_DONE || m->remote == B_CANCEL) && now-m->changed_at > 12000)
            return B_IDLE;
    }
    return m->remote;
}

bool buddy_screen_timeout_valid(unsigned seconds) {
    return seconds == 0 || (seconds >= 60 && seconds <= 3600);
}
buddy_display_mode_t buddy_display_mode(uint64_t now, uint64_t activity_at,
                                        unsigned timeout_seconds, bool busy) {
    if (busy || now < activity_at) return B_DISPLAY_ACTIVE;
    uint64_t idle = now - activity_at;
    if (buddy_screen_timeout_valid(timeout_seconds) && timeout_seconds &&
        idle >= (uint64_t)timeout_seconds * 1000) return B_DISPLAY_OFF;
    return idle >= 90000 ? B_DISPLAY_DIM : B_DISPLAY_ACTIVE;
}
unsigned buddy_display_interval(buddy_display_mode_t mode) {
    return mode == B_DISPLAY_OFF ? 0 : mode == B_DISPLAY_DIM ? 200 : 33;
}

bool buddy_screen_key(bool *locked, bool asleep, bool lock_gesture) {
    if (*locked || asleep) {
        *locked=false;
        return true;
    }
    if (lock_gesture) {
        *locked=true;
        return true;
    }
    return false;
}
