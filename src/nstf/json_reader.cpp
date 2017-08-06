#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <assert.h>
#include <json/json.h>
#include "common/base64.h"
#include <string.h>
#include "44bsd/tcp_socket.h"
#include "json_reader.h"

// make the class singelton
CJsonData* CJsonData::m_pInstance = NULL;

bool CJsonData::parse_file(std::string file) {
    Json::Reader reader;
    std::ifstream t(file);
    std::string m_msg((std::istreambuf_iterator<char>(t)),
                                    std::istreambuf_iterator<char>());
    bool rc = reader.parse(m_msg, m_val, false);

    if (!rc) {
        std::cout << "Failed parsing json file " << file << std::endl;
        return false;
    }

    return true;
}

void CJsonData::dump() {
    std::cout << m_val << std::endl;
}

uint16_t CJsonData::get_buf_index(uint16_t temp_index, uint16_t cmd_index, int side) {
    std::string temp_str;
    Json::Value cmd;

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }
    try {
        cmd = m_val["templates"][temp_index][temp_str]
            ["program"]["commands"][cmd_index];

    } catch(std::exception &e) {
        assert(0);
    }

    assert(cmd["name"] == "tx");

    return cmd["buf_index"].asInt();
}

std::string CJsonData::get_buf(uint16_t temp_index, uint16_t cmd_index, int side) {
    std::string temp_str;
    Json::Value cmd;

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }
    try {
        cmd = m_val["templates"][temp_index][temp_str]
            ["program"]["commands"][cmd_index];

    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "tx");

    uint16_t buf_index = cmd["buf_index"].asInt();
    std::string output = m_val["buf_list"][buf_index].asString();

    return base64_decode(output);
}

tcp_app_cmd_enum_t CJsonData::get_cmd(uint16_t temp_index, uint16_t cmd_index, int side) {
    std::string temp_str;
    Json::Value cmd;

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }
    try {
        cmd = m_val["templates"][temp_index][temp_str]
        ["program"]["commands"][cmd_index];
    } catch(std::exception &e) {
        return tcNO_CMD;
    }

    if (cmd["name"] == "tx") {
        return tcTX_BUFFER;
    }
    if (cmd["name"] == "rcv")
        return tcRX_BUFFER;

    return tcNO_CMD;
}

uint32_t CJsonData::get_num_bytes(uint16_t temp_index, uint16_t cmd_index, int side) {
    std::string temp_str;
    Json::Value cmd;

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }
    try {
        cmd = m_val["templates"][temp_index][temp_str]
            ["program"]["commands"][cmd_index];

    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "rcv");

    return cmd["min_bytes"].asInt();
}


/*
 * Fill program from json file info
 * prog - command to fill
 * temp_index - template index
 * side - 0 - client, 1 - server
 * Return true if prog was filled, false if there was an error
 */
bool CJsonData::get_prog(CTcpAppProgram *prog, uint16_t temp_index, int side) {
    CTcpAppCmd cmd;
    uint16_t cmd_index = 0;
    std::string buf_from_json;
    tcp_app_cmd_enum_t cmd_type;

    do {
        cmd_type = get_cmd(temp_index, cmd_index, side);

        switch(cmd_type) {
        case tcNO_CMD:
            break;
        case tcTX_BUFFER:
            cmd.u.m_tx_cmd.m_buf = m_tcp_data.m_buf_list[get_buf_index(temp_index, cmd_index, side)];
            cmd.m_cmd = tcTX_BUFFER;
            prog->add_cmd(cmd);
            break;
        case tcRX_BUFFER:
            cmd.m_cmd = tcRX_BUFFER;
            cmd.u.m_rx_cmd.m_flags = CTcpAppCmdRxBuffer::rxcmd_WAIT;
            cmd.u.m_rx_cmd.m_rx_bytes_wm = get_num_bytes(temp_index, cmd_index, side);
            prog->add_cmd(cmd);
            break;
        case tcDELAY:
            break;
        case tcRESET:
            break;
        default:
            assert(0);
        }

        cmd_index++;
    } while (cmd_type != tcNO_CMD);

    return true;
}

void CJsonData::convert_bufs() {
    CMbufBuffer *tcp_buf;
    std::string json_buf;
    uint16_t buf_len;

    for (int buf_index = 0; buf_index < m_val["buf_list"].size(); buf_index++) {
        tcp_buf = new CMbufBuffer();
        assert(tcp_buf);
        json_buf = m_val["buf_list"][buf_index].asString();
        std::string temp_str = base64_decode(json_buf);
        buf_len = temp_str.size();
        utl_mbuf_buffer_create_and_copy(0,tcp_buf, 2048, (uint8_t *)(temp_str.c_str()), buf_len);
        m_tcp_data.m_buf_list.push_back(tcp_buf);
    }

    m_tcp_data.dump(stdout);
}

void CJsonData::clear() {
    m_tcp_data.free();
}

void CTcpData::dump(FILE *fd) {
    fprintf(fd, "buf list:\n");
    for (int i = 0; i < m_buf_list.size(); i++) {
        fprintf(fd, "*******%d*******\n", i);
        m_buf_list[i]->Dump(fd);
    }
}

void CTcpData::free() {
    for (int i = 0; i < m_buf_list.size(); i++) {
        m_buf_list[i]->Delete();
        delete m_buf_list[i];
    }

    m_buf_list.clear();
}
