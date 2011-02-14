% Code density/histofgram test to calculate INL and DNL require a large number of samples.
% Step 1: Apply the same sine wave input slightly clipped by the ADC. Hard
% coded for 14bit converter...can be changed as required
% Run the following program.
% Copyright Au/Hofner, Maxim Integrated Products, 120 San Gabriel Drive, Sunnyvale, CA94086
% This program is believed to be accurate and reliable. This program may get altered without prior notification.
% Modifications by Graham Dennis, Diamond Light Source Ltd Jan 2011

numbit=15; % ADC resolution in bits

% "file_labview" is a flag to indicate whether the files are from labview or not, as this
% determines the file reading mode
%file_labview=0; % matlab generated sinewave   
%file_labview=1; % labview file 
file_labview=2; % xspress2 file
%close all

for files=1:1
    files
    filename='';
    %filename=strcat('E:\ad9460_run',num2str(files),'.asc');
    
if(file_labview==1)
    % LABVIEW
    max_num_files=200; % the number of file segments from labview
    adc_data=0; % the (eventual) trimmed adc data from multiple files
    N=16; % Number of ADC bits
    numbit=N;
    vfs=10; % Full scale input of ADC +/-
else
    % XSPRESS2 or matlab generated sinewave
    N=15; % Number of ADC bits
    numbit=N;
    vfs=2; % Full scale input of ADC +/-
end

%---------------------------------------------------------------------
% External File Reading
%---------------------------------------------------------------------

if(file_labview==1)
    % Files are from labview
    %---------------------------------------------------------------------
    % adc data is split into a number of sub-files (=max_num_files).
    % Each should be read and concatonated with previous data.
    % However the data should be trimmed to remove variable
    % start and end positions of the first and last partial sinewave in the
    % data set...this will allow the fully assembled adc data to appear to
    % be continous
    %----------------------------------------------------------------------
    for file_num=1:(max_num_files)
        file_num
        file_name=strcat('N:\ADC Evaluation\Results\2010-04-27\100kHz-2\adc',num2str(file_num),'.txt'); % construct file name
        raw_adc_data=importdata(file_name); % get data from file
        first_zero=find(raw_adc_data==0); % effectively find first and last index points in data file where zeros exist 
        % now find the last occurrence of zero for the first clipped waveform
        first_zero_index=first_zero(1,1);
        while(first_zero(first_zero_index,1)==(first_zero(first_zero_index+1,1)-1))
            first_zero_index=first_zero_index+1;
        end
        % now that we can concatonate multiple data files together to make a
        % continous adc datafile
        raw_data_zeroed=raw_adc_data((first_zero_index+1):first_zero(end,1));
        adc_data=[adc_data;raw_data_zeroed];
    end
elseif(file_labview==2)
    % Files are NOT from labview
    % In this case the data is collected via XSPRESS2
    %readdata= importdata(filename);
    %data=readdata.data;
    %adc_data=[];
    %num_cols_data=size(data); % Allows for varied multiple collumns of data produced by STFC's "imgd" application 
    %for data_row=2:num_cols_data(1,2)
    %    adc_data=[adc_data;data(1:end,data_row)];
    %end
    adc_data=data;
else
    % Generate a perfect over-range sinewave with additional approximation of dnl
    %x = -pi*100:1/(18309):pi*100;
    x = -pi*100:1/(1809):pi*3000;
    adc_data=sin(x);
    %adc_data_sin=adc_data;
    over_range_percent=1;
    %rand_dnl=randn(1,length(adc_data))*1.3;
    rand_dnl=0;
    adc_data=round(adc_data*((1+over_range_percent/100)*2.^(N-1))+2.^(N-1)+rand_dnl-34);
    % Now clip both ends
    gg=find(adc_data<=0);
    adc_data(gg)=0;
    hh=find(adc_data>=(2.^N)-1);
    adc_data(hh)=(2.^N)-1;
    adc_data=adc_data';
    %plot(adc_data)
    %figure
end
disp('Finished reading data. Starting to process ADC data...');
disp(' ')

MT=length(adc_data);

% code_count=zeros(1,2^N);
% for i=1:MT,
%    code_count(adc_data(i)+1)=code_count(adc_data(i)+1) + 1;
% end

histogram_data=zeros(1,2.^N); % initialise histogram vector
% Histogram Data...
adc_data=adc_data+1; % just offsets adc code space by 1, so 0-16383 becomes 1-16384
% this means that histogram bin 1 contains the "all zeros" counts and bin
% 16384 contains the "all ones" counts
histogram_data=hist(adc_data,2.^N);
disp('Data histogramming complete. Check that ADC input is clipped...');
disp(' ')

% check that input is clipped
if histogram_data(1) == 0 | histogram_data(2^N) == 0 | ...
  (histogram_data(1) < histogram_data(2)) | (histogram_data(2^N-1) > histogram_data(2^N))
   disp('ADC not clipping ... Increase sinewave amplitude!');
   break;
else
    disp('ADC input is clipped. Data is good. Calculating offset...'); 
    disp(' ')
end

% check dc offset by looking at number of samples above and below mid scale
% and calculate the offset that will need to be applied to the sinewave pdf
offset=0;
offset_flag=0;

lower=sum(histogram_data(1,1:(2.^(N-1))));
upper=sum(histogram_data(1,(2.^(N-1)+1):(2.^N)));
if(lower>upper)
    offset=offset-1;
    offset_dir=1;
else
    offset=offset+1;
    offset_dir=-1;
end
% offset adjustment
while(offset_flag==0)
    lower=sum(histogram_data(1,1:(2.^(N-1)+offset)));
    upper=sum(histogram_data(1,(2.^(N-1)+1+offset):(2.^N)));
    old_offset_dir=offset_dir;
    if(lower>upper)
        offset=offset-1;
        offset_dir=1;
    else
        offset=offset+1;
        offset_dir=-1;
    end
    if(old_offset_dir~=offset_dir)
        offset_flag=1;
    end
end
% need to tweak offset adjustment when the offset is negative...
if(offset<0)
    offset=offset-1;
end

mid_code=2^(N-1)+offset;
disp('Offset calculated. Now construct Sinewave PDF...');
disp(' ')

A=max(mid_code,2^N-1-mid_code)+0.1; % Initial estimate of amplitude
vin=(0:2^N-1)-mid_code; % distance of codes to mid point
sin2ramp=1./(pi*sqrt(A^2*ones(size(vin))-vin.*vin));

% keep on increasing estimated amplitude until ideal geneated sinewave pdf
% contains more samples than the measured waveform
while sum(histogram_data(2:2^N-1)) < MT*sum(sin2ramp(2:2^N-1))
  A=A+0.1;
  sin2ramp=1./(pi*sqrt(A^2*ones(size(vin))-vin.*vin));
end

disp('You Have Applied a Sine Wave of (dBFS): ');
Amplitude=A/(2^N/2)
disp('...with an offset of (bits): ');
offset

figure;
%plot([0:2^N-1],histogram_data,[0:2^N-1],sin2ramp*MT);
%title('CODE HISTOGRAM - SINE WAVE');
%xlabel('DIGITAL OUTPUT CODE');
%ylabel('COUNTS');
%axis([0 2^N-1 0 max(histogram_data(2),histogram_data(2^N-1))]);

hold all
bar([0:2^N-1],histogram_data)
title(strcat('Histogram of ADC codes for ',num2str(MT),' samples'))
plot([0:2^N-1],sin2ramp*MT,'r','LineStyle','none','Marker','.','MarkerSize',15)
xlabel('ADC Output Code');
ylabel('Counts');
axis([0 2^N-1 0 max(histogram_data(2),histogram_data(2^N-1))]);
%plot([0:2^N-1],histogram_data)


histogram_data_norm=histogram_data(2:2^N-1)./(MT*sin2ramp(2:2^N-1)); 
%figure;
%plot([1:2^N-2],histogram_data_norm);
%title('CODE HISTOGRAM - NORMALIZED')
%xlabel('DIGITAL OUTPUT CODE');
%ylabel('NORMALIZED COUNTS');

dnl=histogram_data_norm-1;
inl=zeros(size(dnl));
for j=1:size(inl')
   inl(j)=sum(dnl(1:j));
end

%figure;
%plot(inl)

%End-Point fit INL
%[p,S]=polyfit([1,2^N-2],[inl(1),inl(2^N-2)],1);

%Best-straight-line fit INL
[p,S]=polyfit([1:2^N-2],inl,1);
inl=inl-p(1)*[1:2^N-2]-p(2);

disp('End Points Eliminated for DNL and INL Calculations');
figure;
plot([1:2^N-2],dnl);
grid on;
title('DIFFERENTIAL NONLINEARITY vs. DIGITAL OUTPUT CODE');
xlabel('DIGITAL OUTPUT CODE');
ylabel('DNL (LSB)');
figure;
plot([1:2^N-2],inl);
grid on;
title('INTEGRAL NONLINEARITY vs. DIGITAL OUTPUT CODE');
xlabel('DIGITAL OUTPUT CODE');
ylabel('INL(LSB)');

inl_all(files,1:length(inl))=inl;

end % files

figure;
plot([1:2^N-2],inl_all');
grid on;
title('INTEGRAL NONLINEARITY vs. DIGITAL OUTPUT CODE');
xlabel('DIGITAL OUTPUT CODE');
ylabel('INL(LSB)');
