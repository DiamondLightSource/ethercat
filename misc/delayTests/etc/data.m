%
%  Delay measurements on PC30 and PC32
%
% measurements in Tektronix TDS 2024 Oscilloscope
% The time scale of the oscilloscope is 500 microseconds per box (major
% division), with a minor division of 100 microseconds
%
% Time in microseconds
pc30 = [ 4.2, 4.4, 5.4, 4.8, 4.5, ...
         5.2, 4.3, 8.4, 4.9, 5.6,  ...
         5.3   ] * 500 ;
pc32 = [ 5.6, 4.7, 4.9, 5.1, 5.1, ...
         5.7, 5.5, 4.8, 4.9, 4.5, ...
         5.4, 4.4 ] * 500 ;
x30=1:length(pc30);
x32=1:length(pc32);
plot(x30,pc30, 'b-.', x32,pc32, 'r-o');

disp(sprintf('Mean pc30 is %f', mean(pc30) ));
disp(sprintf('STD pc30 is %f', std(pc30) ));
disp(sprintf('Mean pc32 is %f', mean(pc32) ));
disp(sprintf('STD pc32 is %f', std(pc32) ));

% OUTPUT:
%Mean pc30 is 2590.909091
%STD pc30 is 583.874208
%Mean pc32 is 2525.000000
%STD pc32 is 213.733054