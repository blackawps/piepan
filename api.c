/*
 * piepie - bot framework for Mumble
 *
 * Author: Tim Cooper <tim.cooper@layeh.com>
 * License: MIT (see LICENSE)
 *
 * This file contains native functions that are called from the piepan_impl.lua
 * file.
 */

// TODO:  convert lua_tostring to lua_tolstring?
// TODO:  get rid of selfs and only pass non-tables?
// TODO:  use Lua user data in place of mallocing it ourselves?

int
api_User_send(lua_State *lua)
{
    // [self, message]
    MumbleProto__TextMessage msg = MUMBLE_PROTO__TEXT_MESSAGE__INIT;
    uint32_t session;
    lua_getfield(lua, -2, "session");
    session = lua_tointeger(lua, -1);
    msg.n_session = 1;
    msg.session = &session;
    msg.message = (char *)lua_tostring(lua, -2);
    sendPacket(PACKET_TEXTMESSAGE, &msg);
    return 0;
}

int
api_User_kick(lua_State *lua)
{
    // [self, string reason]
    MumbleProto__UserRemove msg = MUMBLE_PROTO__USER_REMOVE__INIT;
    lua_getfield(lua, -2, "session");
    msg.session = lua_tointeger(lua, -1);
    if (lua_isstring(lua, -2)) {
        msg.reason = (char *)lua_tostring(lua, -2);
    }
    sendPacket(PACKET_USERREMOVE, &msg);
    return 0;
}

int
api_User_ban(lua_State *lua)
{
    // [self, string reason]
    MumbleProto__UserRemove msg = MUMBLE_PROTO__USER_REMOVE__INIT;
    lua_getfield(lua, -2, "session");
    msg.session = lua_tointeger(lua, -1);
    if (lua_isstring(lua, -2)) {
        msg.reason = (char *)lua_tostring(lua, -2);
    }
    msg.has_ban = true;
    msg.ban = true;
    sendPacket(PACKET_USERREMOVE, &msg);
    return 0;
}

int
api_User_moveTo(lua_State *lua)
{
    // [self, int channel_id]
    MumbleProto__UserState msg = MUMBLE_PROTO__USER_STATE__INIT;
    msg.channel_id = lua_tointeger(lua, -1);
    lua_getfield(lua, -2, "session");
    msg.session = lua_tointeger(lua, -1);
    sendPacket(PACKET_USERSTATE, &msg);
    return 0;
}

int
api_Channel_send(lua_State *lua)
{
    // [self, message]
    MumbleProto__TextMessage msg = MUMBLE_PROTO__TEXT_MESSAGE__INIT;
    uint32_t channel;
    lua_getfield(lua, 1, "id");
    msg.message = (char *)lua_tostring(lua, -2);
    channel = lua_tointeger(lua, -1);
    msg.n_channel_id = 1;
    msg.channel_id = &channel;
    sendPacket(PACKET_TEXTMESSAGE, &msg);
    return 0;
}

int
api_Timer_new(lua_State *lua)
{
    // [id, timeout, table]
    int id;
    int timeout;
    UserTimer *timer;
    id = lua_tonumber(lua, -3);
    timeout = lua_tonumber(lua, -2);

    timer = (UserTimer *)malloc(sizeof(UserTimer));
    if (timer == NULL) {
        return 0;
    }
    lua_pushlightuserdata(lua, timer);
    lua_setfield(lua, -2, "ev_timer");

    timer->id = id;

    ev_timer_init(&timer->ev, user_timer_event, timeout, 0.);
    ev_timer_start(ev_loop_main, &timer->ev);
    return 0;
}

int
api_Timer_cancel(lua_State *lua)
{
    // [ev_timer]
    UserTimer *timer;
    timer = (UserTimer *)lua_touserdata(lua, -1);
    ev_timer_stop(ev_loop_main, &timer->ev);
    free(timer);
    return 0;
}

static void *
api_Thread_worker(void *arg)
{
    UserThread *user_thread = (UserThread *)arg;
    lua_getglobal(user_thread->lua, "piepan");
    lua_getfield(user_thread->lua, -1, "Thread");
    lua_getfield(user_thread->lua, -1, "_implExecute");
    lua_pushnumber(user_thread->lua, user_thread->id);
    lua_call(user_thread->lua, 1, 0);
    write(user_thread_pipe[1], &user_thread->id, sizeof(int));
    return NULL;
}

int
api_Thread_new(lua_State *lua)
{
    // [Thread, id]
    UserThread *user_thread;
    user_thread = (UserThread *)malloc(sizeof(UserThread));
    if (user_thread == NULL) {
        return 0;
    }
    user_thread->id = lua_tonumber(lua, -1);
    user_thread->lua = lua_newthread(lua);
    lua_setfield(lua, -3, "lua");
    lua_pushlightuserdata(lua, user_thread);
    lua_setfield(lua, -3, "userthread");
    pthread_create(&user_thread->thread, NULL, api_Thread_worker, user_thread);
    return 0;
}

int
api_disconnect(lua_State *lua)
{
    kill(0, SIGINT);
    return 0;
}

void
api_init(lua_State *lua)
{
    // piepan
    lua_newtable(lua);

    // piepan.User
    lua_newtable(lua);
    // piepan.User:send(message)
    lua_pushcfunction(lua, api_User_send);
    lua_setfield(lua, -2, "send");
    // piepan.User:kick(reason)
    lua_pushcfunction(lua, api_User_kick);
    lua_setfield(lua, -2, "kick");
    // piepan.User:ban(reason)
    lua_pushcfunction(lua, api_User_ban);
    lua_setfield(lua, -2, "ban");
    // piepan.User:moveTo(channel)
    lua_pushcfunction(lua, api_User_moveTo);
    lua_setfield(lua, -2, "moveTo");
    lua_setfield(lua, -2, "User");

    // piepan.Channel
    lua_newtable(lua);
    // piepan.Channel:send(message)
    lua_pushcfunction(lua, api_Channel_send);
    lua_setfield(lua, -2, "send");
    lua_setfield(lua, -2, "Channel");

    // piepan.Message
    lua_newtable(lua);
    lua_setfield(lua, -2, "Message");

    // piepan.Timer
    lua_newtable(lua);
    // piepan.Timer.new()
    lua_pushcfunction(lua, api_Timer_new);
    lua_setfield(lua, -2, "new");
    // piepan.Timer:cancel()
    lua_pushcfunction(lua, api_Timer_cancel);
    lua_setfield(lua, -2, "cancel");
    lua_setfield(lua, -2, "Timer");

    // piepan.Thread
    lua_newtable(lua);
    // piepan.Thread.new()
    lua_pushcfunction(lua, api_Thread_new);
    lua_setfield(lua, -2, "new");
    lua_setfield(lua, -2, "Thread");

    // piepan.disconnect
    lua_pushcfunction(lua, api_disconnect);
    lua_setfield(lua, -2, "disconnect");

    lua_setglobal(lua, "piepan");
}