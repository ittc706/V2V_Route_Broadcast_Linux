clear all;
close all;
clc;


figId=1;

%% PRR
PackageLossDistance=load('failed_distance.txt');
PackageTransimitDistance=load('success_distance.txt');

%IntersectDistance=intersect(unique(PackageLossDistance),unique(PackageTransimitDistance));
IntersectDistance=0:20:max(PackageLossDistance);

[numPackageLossDistance,centerPackageLossDistance]=hist(PackageLossDistance',IntersectDistance);
[numPackageTransimitDistance,centerPackageTransimitDistance]=hist(PackageTransimitDistance',IntersectDistance);

numPackageLossDistance=numPackageLossDistance./(numPackageTransimitDistance+numPackageLossDistance);

figure(figId)
figId=figId+1;
plot(centerPackageLossDistance,1-numPackageLossDistance,'bo-','LineWidth',2);
title('PDR','LineWidth',2);
xlabel('Distance(m)','LineWidth',2);
ylabel('Drop Rate','LineWidth',2);
axis([0 3000 0 1]);
grid on;