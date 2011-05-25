function ectest

while 1;
    % samples = lcaGet('JRO:RS');
    samples = lcaGet('JRECTEST:3:ADC1_WAVEFORM');
    if 0
    cyc = lcaGet('JRO:RC');
    cyc = [0 diff(cyc)];
    c2 = repmat(cyc, 11, 1);
    c2 = c2(:);
    
    subplot(2,1,1);
    plot(c2 * max(abs(samples)), 'r');
    hold on;
    plot(samples, '.-');
    hold off;
    end
    subplot(2,1,1);
    % sx = samples(1:10084);
    sx = samples;
    % sx = samples(1:5143);
    plot(sx, '.-');
    subplot(2,1,2);
    fx = 10083.6975;
    plot((0:length(sx)-1)/length(sx)*fx, abs(fft(sx)));
    xlim([0 100]);
    % xlim([3950 4050]);
    % fx = 5143.1510;
    fdet = tunenewt(sx - mean(sx)) * fx;
    title(sprintf('%f', fdet));
    pause(0.1);
    % assignin('base', 'a', sx);
end

