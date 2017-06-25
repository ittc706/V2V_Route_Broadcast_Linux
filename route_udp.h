#pragma once
#include<iostream>
#include<vector>
#include<list>
#include<queue>
#include<set>
#include<map>
#include<utility>
#include<random>
#include<string>
#include"route.h"
#include"config.h"
#include"reflect/object.h"
#include"reflect/context.h"

using namespace std;

enum node_type {
	VUE,
	RSU
};

/*
* 对于路由层，不涉及车辆，将车辆抽象为node
* 对于一个节点，收发矛盾。即同一时刻只能收，或者只能发
* 当一个节点处于收发状态时，回绝一切请求
* 当节点处于收状态时，该节点的作为源节点的信息也将在收完毕后再进行发送，即排在转发消息之后
* 若节点处于空闲状态，且同时收到两个或多个转发请求，随机应答一个，拒绝其他(可以扩展为优先级应答)
*/

class route_udp_node;

class route_udp_route_event {
	/*
	* 事件存在的起始时间
	*/
private:
	int m_start_tti = 0;

public:
	int get_start_tti() {
		return m_start_tti;
	}

public:
	static int s_event_count;

	/*
	* 源节点
	*/
private:
	const int m_origin_source_node_id;
public:
	int get_origin_source_node_id() {
		return m_origin_source_node_id;
	}

	/*
	* 目的节点,广播时目的节点为-1
	*/
private:
	const int m_final_destination_node_id;
public:
	int get_final_destination_node_id() {
		return m_final_destination_node_id;
	}

	/*
	* 到目前为止的跳数
	*/
public:
	int m_hop = 0;

	/*
	* 事件id
	*/
private:
	const int m_event_id;
public:
	int get_event_id() { return m_event_id; }

	/*
	* 数据包大小，映射成TTI
	*/
private:
	const int m_tti_num;
public:
	int get_tti_num() {
		return m_tti_num;
	}

public:
	/*
	* 构造函数，提供给事件触发模块调用
	*/
	route_udp_route_event(int t_source_node, int t_destination_node, int current_tti, int event_id, int hop) :
		m_event_id(event_id),
		m_origin_source_node_id(t_source_node),
		m_final_destination_node_id(t_destination_node),
		m_hop(hop),
		m_start_tti(current_tti),
		m_tti_num(((tmc_config*)context::get_context()->get_bean("tmc_config"))->get_package_num()) {
	}

};

class route_udp_link_event {
	friend class route_udp_node;
	friend class route_udp;

private:
	/*
	* 当前链路源节点id
	*/
	const int m_source_node_id;
public:
	int get_source_node_id() {
		return m_source_node_id;
	}

	/*
	* 当前链路目的节点id
	*/
private:
	const int m_destination_node_id;
public:
	int get_destination_node_id() {
		return m_destination_node_id;
	}

	/*
	* 数据包总数
	* 即要发送的数据封装成IP数据包的数量，这些IP数据包丢失任意一个，那么整个数据包就算丢失
	*/
private:
	const int m_tti_num;


	/*
	* 占用的pattern编号
	*/
private:
	int m_pattern_idx;
	void set_pattern_idx(int t_pattern_idx) {
		m_pattern_idx = t_pattern_idx;
	}
public:
	int get_pattern_idx() {
		return m_pattern_idx;
	}

	/*
	* 标记本跳当前时刻传输到底几个TTI
	*/
private:
	int m_tti_idx = 0;
public:
	int get_tti_idx() { return m_tti_idx; }

	/*
	* 标记本跳是否传输完毕(无论是否发生丢包)
	*/
private:
	bool m_is_finished = false;
public:
	bool is_finished() { return m_is_finished; }

	/*
	* 标记本跳是否发生丢包
	*/
private:
	bool m_is_loss = false;
public:
	bool get_is_loss() { return m_is_loss; }

public:
	route_udp_link_event(int t_source_node_id, int t_destination_node_id, int t_pattern_idx, int t_package_num) :
		m_source_node_id(t_source_node_id),
		m_destination_node_id(t_destination_node_id),
		m_pattern_idx(t_pattern_idx),
		m_tti_num(t_package_num) {}

	/*
	* 进行数据包的发送
	*/
	void transimit();
};

class route_udp_node {
	friend class route_udp;
public:
	map<int, double> success_route_event;
	map<int, double> failed_route_event;
public:
	int m_broadcast_time;//下次广播的时间
public:
	node_type s_node_type;//节点类型，包括车辆节点和RSU节点
	int s_rsu_pattern_id;//RSU节点采用固定频段发送
private:
	/*
	* 正在发送的link_event指针，每个子信道上一个
	*/
	std::vector<route_udp_link_event*> sending_link_event;

	/*
	* 节点编号
	*/
	static int s_node_count;

	/*
	* 随机数引擎
	*/
	static std::default_random_engine s_engine;

	/*
	* 正在发送(强调一下:发状态的节点)的node节点
	* 外层下标为pattern编号
	*/
	static std::vector<std::set<int>> s_node_id_per_pattern;
public:
	static const std::set<int>& get_node_id_set(int t_pattern_idx);

	/*
	* 当前节点待发送车辆队列
	*/
private:
	std::queue<route_udp_route_event*> m_send_event_queue;
public:
	int get_send_event_queue_length() {
		return m_send_event_queue.size();
	}
public:
	void offer_send_event_queue(route_udp_route_event* t_event) {
		m_send_event_queue.push(t_event);
	}
	route_udp_route_event* poll_send_event_queue() {
		route_udp_route_event* temp = m_send_event_queue.front();
		m_send_event_queue.pop();
		return temp;
	}
	route_udp_route_event* peek_send_event_queue() {
		return m_send_event_queue.front();
	}
	bool is_send_event_queue_empty() {
		return m_send_event_queue.empty();
	}

private:
	/*
	* 节点id
	*/
	const int m_id = s_node_count++;
public:
	int get_id() {
		return m_id;
	}

public:
	/*
	* 构造函数
	*/
	route_udp_node();

public:
	/*
	* 选择请求转发的车辆以及相应的频段
	* first字段为车辆id
	* second字段为频段编号
	* 任意一个字段为-1，就代表选择失败
	*/
	std::pair<int, int> select_relay_information();
};

class v2x_time;
class gtt;
class wt;
class rrm_config;
class tmc_config;
class route_config;

class route_udp :public route {
	REGISTE_MEMBER_HEAD(route_udp)
public:
	int s_car_num;//车辆总数
	int s_rsu_num;//路边单元总数
public:
	static const double s_rsu_pattern[24 * 2];
private:
	/*
	* 随机数引擎
	*/
	static std::default_random_engine s_engine;

	/*
	* 日志输出流
	*/
	static std::ofstream s_logger_link_pdr_distance;
	static std::ofstream s_logger_delay;

private:
	/*
	* 节点数组
	*/
	route_udp_node* m_node_array;
public:
	route_udp_node* get_node_array() {
		return m_node_array;
	}

private:

	/*
	* 成功/失败传输的事件个数
	*/
	int m_success_route_event_num = 0;

	int m_failed_route_event_num = 0;

	int m_broadcast_num = 0;

	int m_event_num = 0;
public:

	int get_success_route_event_num(){
		return m_success_route_event_num;
	}
	int get_failed_route_event_num() {
		return m_failed_route_event_num;
	}
	int get_broadcast_num() {
		return m_broadcast_num;
	}
	int get_event_num() {
		return m_event_num;
	}

private:
	v2x_time* m_time;
	gtt* m_gtt;
	wt* m_wt;
	rrm_config* m_rrm_config;
	tmc_config* m_tmc_config;
	route_config* m_route_config;

	void set_time(object* t_time) {
		m_time = (v2x_time*)t_time;
	}
	void set_gtt(object* t_gtt) {
		m_gtt = (gtt*)t_gtt;
	}
	void set_wt(object* t_wt) {
		m_wt = (wt*)t_wt;
	}
	void set_rrm_config(object* t_rrm_config) {
		m_rrm_config = (rrm_config*)t_rrm_config;
	}
	void set_tmc_config(object* t_tmc_config) {
		m_tmc_config = (tmc_config*)t_tmc_config;
	}
	void set_route_config(object* t_route_config) {
		m_route_config = (route_config*)t_route_config;
	}
public:
	v2x_time* get_time() override {
		return m_time;
	}

	gtt* get_gtt() override {
		return m_gtt;
	}

	wt* get_wt() override {
		return m_wt;
	}

	rrm_config* get_rrm_config() override {
		return m_rrm_config;
	}

	tmc_config* get_tmc_config() override {
		return m_tmc_config;
	}

	route_config* get_route_config() override {
		return m_route_config;
	}

	void initialize() override;

	void process_per_tti() override;

	void update_route_table_from_physics_level() override;

private:
	/*
	* 随机触发事件
	*/
	void event_trigger();

	/*
	* 触发要开始发送的事件
	*/
	void start_sending_data();

	/*
	* 传输当前TTI存在的事件
	*/
	void transmit_data();

	/*
	* 选择RSU
	*/
	vector<int> select_rsu(int vueid);

};
