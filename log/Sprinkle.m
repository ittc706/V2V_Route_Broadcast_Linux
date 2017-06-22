[data1,data2] = textread('vue_coordinate.txt','%n%n');
plot(data1,data2,'bx');
hold on;
[data3,data4]=textread('rsu_coordinate.txt','%n%n');
plot(data3,data4,'rx');
