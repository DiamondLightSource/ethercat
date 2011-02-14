function wav = plotdump2(filename)

dump = load(filename);
good = find(diff(dump(:,4)) ~= 0);
dump = dump(good, :);
L = size(dump, 1);
ofs = (0:L-1)*10;

wav = zeros(10*L, 1);

hold off
for n = 1:10
    plot(ofs+n, dump(:, 6+n-1), '.');
    wav(ofs+n) = dump(:, 6+n-1);
    hold all;
end

