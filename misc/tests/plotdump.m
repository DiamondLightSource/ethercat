function plotdump(filename)
dump = load(filename);
d2 = dump(end-50:end, :);

% major sample spacing is 1ms
% minor sample spacing is 1/100 ms
nd = size(d2, 2)-1;

t00 = d2(1,1);

for m = 1:size(d2, 1)
    t0 = d2(m, 1) - t00;
    data = d2(m, 2:end);
    ts = t0 + (1:nd) * 0.01;
    
    plot(ts, data, '.');
    hold all
end
hold off

