/*
* =====================================================================================
*
*       Filename:  tmc.cpp
*
*    Description:  业务模型与控制类实现
*
*        Version:  1.0
*        Created:
*       Revision:
*       Compiler:  VS_2015
*
*         Author:  HCF
*            LIB:  ITTC
*
* =====================================================================================
*/

#include<random>
#include<fstream>
#include"gtt.h"
#include"tmc.h"
#include"vue.h"
#include"vue_physics.h"
#include"config.h"
#include"route_tcp.h"
#include"route_udp.h"
#include"reflect/context.h"


using namespace std;

void tmc::statistic() {
	context* __context = context::get_context();

	ofstream output;
	ofstream s_logger_failed_distance;
	ofstream s_logger_success_distance;

	output.open("log/output.txt");
	s_logger_failed_distance.open("log/failed_distance.txt");
	s_logger_success_distance.open("log/success_distance.txt");

	object* __object = context::get_context()->get_bean("route");

	route_udp* __route_udp = (route_udp*)__object;
	output << "total success event: " << __route_udp->get_success_route_event_num() << endl;
	output << "total failed event: " << __route_udp->get_failed_route_event_num() << endl;
	output << "total broadcast number:" << __route_udp->get_broadcast_num() << endl;
	output << "total event number:" << __route_udp->get_event_num() << endl;

	for (int i = 0; i < vue_physics::get_vue_num(); i++) {
		map<int, double>::iterator failed = __route_udp->get_node_array()[i].failed_route_event.begin();
		while (failed != __route_udp->get_node_array()[i].failed_route_event.end()) {
			s_logger_failed_distance << failed->second << " ";
			failed++;
		}
		map<int, double>::iterator success = __route_udp->get_node_array()[i].success_route_event.begin();
		while (success != __route_udp->get_node_array()[i].success_route_event.end()) {
			s_logger_success_distance << success->second << " ";
			success++;
		}
	}
}