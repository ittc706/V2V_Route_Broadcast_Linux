close all;
clear all;
[data1,data2,data3] = textread('route_udp_link_pdr_distance.txt','%n%n%n','delimiter', ',');

for i=1:1:length(data1)
    if data1(i)==0
        hop1_num=i;
        break;
    end
end

plot(data2(1:hop1_num-1),data3(1:hop1_num-1),'bx');
hold on;
plot(data2(hop1_num:length(data2)),data3(hop1_num:length(data3)),'rx');
hold on;

[data4,data5,data6] = textread('last_location.txt','%n%n%n');
plot(data5,data6,'kx');
hold on;
plot(-3.5,-15.0835,'g*'); 
title('�����㲥600m�ڽ��սڵ�ֲ�','LineWidth',2);
xlabel('������(m)','LineWidth',2);
ylabel('������(m)','LineWidth',2);
legend('һ��','����','δ����','Դ��');
% axis([-800 600 -400 800]);

% plot(-36.1698,225,'kx');