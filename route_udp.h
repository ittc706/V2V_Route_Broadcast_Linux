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

enum route_udp_route_event_type {
	Broadcast,//广播
    Unicast//单播
};

enum route_udp_link_event_loss_reason {
	UNKNOW,//待定
	LOW_SINR,//SINR低于阈值
	DST_IS_SENDING//接收节点正在发送无法接收
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

private:
	/*
	* 包的类型，类似于包头的设置，用来明确不同作用的数据包
	*/
	const route_udp_route_event_type m_route_event_type;
public:
	route_udp_route_event_type get_route_event_type() {
		return m_route_event_type;
	}

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
	* 当前节点(已正确传输到当前节点，即当前节点也在m_through_node_vec之中)
	*/
private:
	int m_current_node_id = -1;
public:
	void set_current_node_id(int t_current_node_id) {
		m_current_node_id = t_current_node_id;
		m_through_node_id_vec.push_back(m_current_node_id);
	}
	int get_current_node_id() {
		return m_current_node_id;
	}

	/*
	* 到目前为止的跳数
	*/
public:
	int m_hop = 1;

	/*
	* 经历的节点列表(只包含成功传输的)
	*/
private:
	std::vector<int> m_through_node_id_vec;
public:
	const std::vector<int>& get_through_node_vec() {
		return m_through_node_id_vec;
	}

	/*
	* 整个从源节点到目的节点是否传递成功
	*/
	bool is_finished() {
		return m_current_node_id == m_final_destination_node_id;
	}

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
	route_udp_route_event(int t_source_node, int t_destination_node,route_udp_route_event_type t_route_event_type,int current_tti,int event_id) :
		m_route_event_type(t_route_event_type),
		m_event_id(event_id),
		m_origin_source_node_id(t_source_node),
		m_final_destination_node_id(t_destination_node),
		m_start_tti(current_tti),
		m_tti_num(((tmc_config*)context::get_context()->get_bean("tmc_config"))->get_package_num()) {
		set_current_node_id(t_source_node);
	}

	/*
	* 转为字符串
	*/
	std::string to_string();
};

class route_udp_link_event {
	friend class route_udp_node;
	friend class route_udp;


private:

	/*
	* 该link_evnent的SINR，传输需要几个TTI，则输出几个TTI上的SINR
	*/
	vector<double> sinr_per_tti;

	/*
	* 该link_evnent传输失败的原因
	*/
	route_udp_link_event_loss_reason m_loss_reason = UNKNOW;
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
private:
	/*
	* 正在发送的link_event指针，若为单播则一个节点同时只能有一个，若为广播则为一个遍历除自己以外所有节点的向量
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

private:
	/*
	* 随机数引擎
	*/
	static std::default_random_engine s_engine;

	/*
	* 日志输出流
	*/
	static std::ofstream s_logger_pattern;
	static std::ofstream s_logger_link;
	static std::ofstream s_logger_event;
	static std::ofstream s_logger_link_pdr_distance;
	static std::ofstream s_logger_delay;

	/*
	* 记录日志
	*/
	static void log_event(int t_origin_node_id, int t_fianl_destination_node_id);

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

public:

	int get_success_route_event_num(){
		return m_success_route_event_num;
	}
	int get_failed_route_event_num() {
		return m_failed_route_event_num;
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

};
