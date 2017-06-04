#include<iostream>
#include<iomanip>
#include<fstream>
#include<sstream>
#include"route_udp.h"
#include"config.h"
#include"gtt.h"
#include"wt.h"
#include"vue.h"
#include"vue_physics.h"
#include"function.h"
#include"reflect/context.h"
#include"time_stamp.h"

using namespace std;

int route_udp_route_event::s_event_count = 0;

default_random_engine route_udp::s_engine;

std::string route_udp_route_event::to_string() {
	stringstream ss;
	for (int i = 0; i < m_through_node_id_vec.size(); i++) {
		ss << "node[" << left << setw(3) << m_through_node_id_vec[i] << "]";
		if (i < m_through_node_id_vec.size() - 1)ss << " -> ";
	}
	ss << endl;
	return ss.str();
}

void route_udp_link_event::transimit() {
	//<Warn>:一跳就在指定频段传输，不再分成多个包分别选择频段传输了

	if (++m_tti_idx == m_tti_num) {
		m_is_finished = true;
	}

	if (get_pattern_idx() < 0 || get_pattern_idx() > 5) throw logic_error("error");
	double sinr = ((wt*)context::get_context()->get_bean("wt"))->calculate_sinr(
		get_source_node_id(),
		get_destination_node_id(),
		get_pattern_idx(),
		route_udp_node::get_node_id_set(get_pattern_idx()));
	sinr_per_tti.push_back(sinr);

	if (sinr < ((rrm_config*)context::get_context()->get_bean("rrm_config"))->get_drop_sinr_boundary()){
		m_is_loss = true;
		m_loss_reason = LOW_SINR;
	}
}

int route_udp_node::s_node_count = 0;

default_random_engine route_udp_node::s_engine(time(NULL));

std::vector<std::set<int>> route_udp_node::s_node_id_per_pattern;

const std::set<int>& route_udp_node::get_node_id_set(int t_pattern_idx) {
	return s_node_id_per_pattern[t_pattern_idx];
}

route_udp_node::route_udp_node() {
	context* __context = context::get_context();
	int interval = ((route_config*)__context->get_bean("route_config"))->get_t_interval();
	uniform_int_distribution<int> u_start_broadcast_tti(0, interval);

	m_broadcast_time = u_start_broadcast_tti(s_engine);//初始化第一次发送周期消息的时间，用于错开干扰
}

pair<int, int> route_udp_node::select_relay_information() {
	pair<int, int> res = make_pair<int, int>(-1, -1);

	//先挑选路由车辆id
	int final_destination_node_id = peek_send_event_queue()->get_final_destination_node_id();
	if (final_destination_node_id != -1) {//判断是否为广播事件，如果是则不需挑选下一跳
		throw logic_error("error");
	}
	
	int pattern_num = ((rrm_config*)context::get_context()->get_bean("rrm_config"))->get_pattern_num();

	//挑选频段，在所有频段上随机选择
	vector<int> candidate;
	for (int pattern_idx = 0; pattern_idx < pattern_num; pattern_idx++) {
			candidate.push_back(pattern_idx);
	}

	if (candidate.size() != 0) {
		//在未占用的频段上随机挑选一个
		uniform_int_distribution<int> u(0, static_cast<int>(candidate.size()) - 1);
		res.second = candidate[u(s_engine)];
	}
	return res;
}

ofstream route_udp::s_logger_pattern;
ofstream route_udp::s_logger_link;
ofstream route_udp::s_logger_event;
ofstream route_udp::s_logger_link_pdr_distance;
ofstream route_udp::s_logger_delay;

void route_udp::log_event(int t_origin_node_id, int t_fianl_destination_node_id) {
	v2x_time* __time = (v2x_time*)context::get_context()->get_bean("time");
	s_logger_event << "TTI[" << left << setw(3) << __time->get_tti() << "] - ";
	s_logger_event << "trigger[" << left << setw(3) << t_origin_node_id << ", ";
	s_logger_event << left << setw(3) << t_fianl_destination_node_id << "]" << endl;

}

void route_udp::initialize() {
	context* __context = context::get_context();
	int vue_num = get_gtt()->get_vue_num();
	m_node_array = new route_udp_node[vue_num];

	s_logger_pattern.open("log/route_udp_pattern_log.txt");
	s_logger_link.open("log/route_udp_link_log.txt");
	s_logger_event.open("log/route_udp_event_log.txt");
	s_logger_link_pdr_distance.open("log/route_udp_link_pdr_distance.txt");
	s_logger_delay.open("log/route_udp_delay.txt");

	route_udp_node::s_node_id_per_pattern = vector<set<int>>(get_rrm_config()->get_pattern_num()+1);
}

void route_udp::process_per_tti() {
	//事件触发
	event_trigger();

    //触发要开始发送的事件
	start_sending_data();

	//传输当前TTI存在的事件
	transmit_data();
}

void route_udp::event_trigger() {
	context* __context = context::get_context();
	int interval = ((route_config*)__context->get_bean("route_config"))->get_t_interval();

	if (get_time()->get_tti() < ((global_control_config*)__context->get_bean("global_control_config"))->get_ntti()) {
		//在初始化时间过后，触发数据传输事件
		for (int origin_source_node_id = 0; origin_source_node_id < route_udp_node::s_node_count; origin_source_node_id++) {
			route_udp_node& source_node = get_node_array()[origin_source_node_id];
			if (get_time()->get_tti() == source_node.m_broadcast_time) {
				get_node_array()[origin_source_node_id].offer_send_event_queue(
					new route_udp_route_event(origin_source_node_id, -1, Broadcast, get_time()->get_tti(), route_udp_route_event::s_event_count++,1)
				);
				log_event(origin_source_node_id, -1);
				source_node.m_broadcast_time += interval;
			}
			source_node.success_route_event[route_udp_route_event::s_event_count-1] = 0;//标记该接收节点已经收到过此事件，避免重复接收
		}
	}
	

	//route_udp_node& source_node = get_node_array()[100];
	//if (get_time()->get_tti() == 1) {
	//	get_node_array()[100].offer_send_event_queue(
	//		new route_udp_route_event(100, -1, Broadcast, get_time()->get_tti(), route_udp_route_event::s_event_count++)
	//	);
	//	log_event(100, -1);
	//}
}

void route_udp::start_sending_data() {
	int pattern_num = ((rrm_config*)context::get_context()->get_bean("rrm_config"))->get_pattern_num();

	//本着同一时刻发消息优先于收消息的原则，所有发消息的事件在传输前先选择传输频段并进行占用
	for (int source_node_id = 0; source_node_id < route_udp_node::s_node_count; source_node_id++) {
		route_udp_node& source_node = get_node_array()[source_node_id];

		if (source_node.sending_link_event.size() == 0) {//当前节点上一个事件已经完成传输或者没有要传输的事件
			
			if (source_node.is_send_event_queue_empty()) continue;//当前车辆待发送事件列表为空，跳过即可

            //如果该事件是单播事件
			if (source_node.m_send_event_queue.front()->get_route_event_type() == Unicast) {

			}

			//如果该事件是广播事件
			else {  
				                           
				//选择频段
				pair<int, int> select_res = source_node.select_relay_information();

				//维护干扰列表
				if (route_udp_node::s_node_id_per_pattern[select_res.second].find(source_node_id) != route_udp_node::s_node_id_per_pattern[select_res.second].end()) throw logic_error("error");

				if (select_res.second < 0 || select_res.second >= pattern_num) throw logic_error("error");

				route_udp_node::s_node_id_per_pattern[select_res.second].insert(source_node_id);

				//对除了该节点以外的其他节点创建链路事件
				for (int dst_id = 0; dst_id < route_udp_node::s_node_count; dst_id++) {
					context *__context = context::get_context();

					if (dst_id == source_node_id||vue_physics::get_distance(source_node.m_send_event_queue.front()->get_origin_source_node_id(), dst_id)>((global_control_config*)__context->get_bean("global_control_config"))->get_max_distance()) continue;
					
					map<int, double>::iterator marked = get_node_array()[dst_id].success_route_event.find(source_node.m_send_event_queue.front()->get_event_id());
					if (marked != get_node_array()[dst_id].success_route_event.end()) continue;//如果某节点已经接收过该事件则不进行传输（减少运算量）

					source_node.sending_link_event.push_back(new route_udp_link_event(
						source_node_id, dst_id, select_res.second, source_node.peek_send_event_queue()->get_tti_num()));
				}
				if (source_node.sending_link_event.size() == 0) {//如果广播接收节点均已完成接收则不再建立广播连接
					route_udp_route_event* temp = source_node.m_send_event_queue.front();
					source_node.m_send_event_queue.pop();
					delete temp;

					//将已经加入干扰列表的车辆id再删掉
					route_udp_node::s_node_id_per_pattern[select_res.second].erase(source_node_id);
				}
			}
		}
	}
}

void route_udp::transmit_data() {
	int pattern_num = ((rrm_config*)context::get_context()->get_bean("rrm_config"))->get_pattern_num();

	//对所有link_event进行第一遍遍历。目的1：传输所有事件。目的2：维护接收节点传输pattern的状态
	for (int source_node_id = 0; source_node_id < route_udp_node::s_node_count; source_node_id++) {
		route_udp_node& source_node = get_node_array()[source_node_id];
		if (source_node.sending_link_event.size() == 0) continue;

		//对当前结点的所有link_event进行遍历传输
		vector<route_udp_link_event*>::iterator it;
		for (it = source_node.sending_link_event.begin(); it != source_node.sending_link_event.end(); it++) {

			route_udp_node& destination_node = get_node_array()[(*it)->get_destination_node_id()];

			//事件传输
			(*it)->transimit();

			//取出当前事件所占用的pattern编号
			int pattern_idx = (*it)->m_pattern_idx;

			if (pattern_idx < 0 || pattern_idx >= pattern_num) throw logic_error("error");

		}
	}

	//对所有link_event进行第二遍遍历，对已经传输完毕的事件进行操作。目的1：统计事件传输成功还是丢失。目的2：修改发送节点和接收节点当前pattern上的状态,维护干扰列表。目的3：销毁link_event，传递route_event
	for (int source_node_id = 0; source_node_id < route_udp_node::s_node_count; source_node_id++) {
		route_udp_node& source_node = get_node_array()[source_node_id];
		if (source_node.sending_link_event.size() == 0) continue;

		//对当前结点的所有link_event进行遍历维护
		vector<route_udp_link_event*>::iterator it;

		bool all_link_event_finished = false;//用于判断所有link_event是否传输完毕，以删除link_event
		for (it = source_node.sending_link_event.begin(); it != source_node.sending_link_event.end(); it++) {

			if ((*it)->is_finished()) {
				all_link_event_finished = true;

				int pattern_idx = (*it)->m_pattern_idx;

				route_udp_node& destination_node = get_node_array()[(*it)->get_destination_node_id()];

				int destination_node_id = destination_node.get_id();

				int origin_node_id = source_node.m_send_event_queue.front()->get_origin_source_node_id();

				//所有link_event处理完毕后，维护干扰列表
				if (it == source_node.sending_link_event.end() - 1){
					if (route_udp_node::s_node_id_per_pattern[pattern_idx].find(source_node_id) == route_udp_node::s_node_id_per_pattern[pattern_idx].end()) throw logic_error("error");
					if (pattern_idx < 0 || pattern_idx >= pattern_num) throw logic_error("error");
					route_udp_node::s_node_id_per_pattern[pattern_idx].erase(source_node_id);
				}

				//判断是否丢包
				if ((*it)->get_is_loss()) {
					if (source_node.m_send_event_queue.front()->get_route_event_type() == Unicast) {//如果是单播

					}
					else {//广播
						//s_logger_link_pdr_distance << 0 << "," << vue_physics::get_distance(origin_node_id, destination_node.get_id()) << endl;//记录失败与当前距离
						context *__context = context::get_context();
						if (vue_physics::get_distance(origin_node_id, destination_node_id) <= ((global_control_config*)__context->get_bean("global_control_config"))->get_max_distance()) {
							map<int, double>::iterator marked = destination_node.failed_route_event.find(source_node.m_send_event_queue.front()->get_event_id());
							map<int, double>::iterator _marked = destination_node.success_route_event.find(source_node.m_send_event_queue.front()->get_event_id());
							if (marked == destination_node.failed_route_event.end() && _marked == destination_node.success_route_event.end()) {//如果该事件没有被接收，则加入标记
								destination_node.failed_route_event[source_node.m_send_event_queue.front()->get_event_id()] = vue_physics::get_distance(origin_node_id, destination_node_id);//标记该接收节点已经收到过此事件，避免重复接收
								m_failed_route_event_num++;
							}
						}
					}

					if (source_node.m_send_event_queue.empty()) throw logic_error("error");

					//所有link_event处理完毕后，删除route_event
					if (it == source_node.sending_link_event.end() - 1) {
						route_udp_route_event* temp = source_node.m_send_event_queue.front();

						//删除route_event
						source_node.m_send_event_queue.pop();

						delete temp;
					}
				}

				else {
					//如果是单播，则更新当前节点id，并判断是否接收成功
					if (source_node.m_send_event_queue.front()->get_route_event_type() == Unicast) {
					}

					//如果是广播，则根据路由算法进行处理
					else {
						//s_logger_link_pdr_distance << 1 << "," << vue_physics::get_distance(origin_node_id, destination_node.get_id()) << endl;//记录接收成功与当前距离

						map<int, double>::iterator marked = destination_node.success_route_event.find(source_node.m_send_event_queue.front()->get_event_id());
						if (marked == destination_node.success_route_event.end()) {//如果该事件没有被接收，则加入标记
							
							destination_node.success_route_event[source_node.m_send_event_queue.front()->get_event_id()] = vue_physics::get_distance(origin_node_id, destination_node_id);//标记该接收节点已经收到过此事件，避免重复接收
							m_success_route_event_num++;

							s_logger_link_pdr_distance << source_node.m_send_event_queue.front()->m_hop << "," << get_gtt()->get_vue_array()[destination_node.get_id()].get_physics_level()->m_absx << "," << get_gtt()->get_vue_array()[destination_node.get_id()].get_physics_level()->m_absy << endl;

							if (source_node.m_send_event_queue.front()->m_hop != 0) {
								if (source_node_id != origin_node_id) {
									int temp = 1;
								}

								destination_node.offer_send_event_queue(
									new route_udp_route_event(origin_node_id, -1, Broadcast, get_time()->get_tti(), source_node.m_send_event_queue.front()->get_event_id(), --(source_node.m_send_event_queue.front()->m_hop))
								);//如果需要继续广播则在接收节点发送队列里加入事件

								
							}

							map<int, double>::iterator failed = destination_node.failed_route_event.find(source_node.m_send_event_queue.front()->get_event_id());
							if (failed != destination_node.failed_route_event.end()) {
								m_failed_route_event_num--;
								destination_node.failed_route_event.erase(failed);
							}
						}
					}

					if (source_node.m_send_event_queue.empty()) throw logic_error("error");

					//所有link_event处理完毕后，删除route_event
					if (it == source_node.sending_link_event.end() - 1) {
						route_udp_route_event* temp = source_node.m_send_event_queue.front();
						source_node.m_send_event_queue.pop();
						delete temp;
					}
				}
			}
		}

		//所有完成的link_event 处理完毕后，删除所有link_event
		if (all_link_event_finished == true) {
			vector<route_udp_link_event*>::iterator it = source_node.sending_link_event.begin();
			while (it != source_node.sending_link_event.end()) {
				delete *it;
				it++;
			}
			source_node.sending_link_event.clear();
		}
	}
}

//简单的根据距离维护邻接表
void route_udp::update_route_table_from_physics_level() {

}
