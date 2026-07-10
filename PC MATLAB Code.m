clc;
clear;

port = "COM3"; % serial port to connect to the microcontroller (change if required)
baud = 115200; % baud rate
NUM_POINTS = 32;
STEP_MM = 300; % x displacement in mm
offfset = 270; % angular offset (sensor starting down)

% start serial connection
try
    s = serialport(port, baud); % open serial connection
    configureTerminator(s, "LF");
    s.Timeout = 0.5;
    flush(s);
catch
    error("Connection failed!"); %error
end

% button to stop and plot the 3D graph
fig = uifigure('Name', 'System Dashboard', 'Position', [100 100 300 150]);
stopBtn = uibutton(fig, 'Text', 'END CAPTURE', ...
    'Position', [75 50 150 50], ...
    'BackgroundColor', [0.7 0.1 0.1], 'FontColor', 'w');

stopBtn.UserData = false;
stopBtn.ButtonPushedFcn = @(btn, event) assignin('base', 'stopNow', true); % set stopping variable if button is pressed
assignin('base', 'stopNow', false);

% command window prints for user instructions
fprintf("Status: Initialized!\n");
fprintf("1. Press PM0 on MSP to start session\n");
fprintf("2. Use PM1 to start slice scan\n");
fprintf("3. Select 'END CAPTURE' to render 3D model\n");

% initalize arrays and variable
all_scans = {};
scan_buf = [];
inScan = false;

% MAIN LOOP
for iter = 1:1e6
    drawnow;
    if stopNow, break; end % stops if button is pressed
    
    if s.NumBytesAvailable == 0 % wait for data to be sent
        pause(0.01);
        continue;
    end
    line = readline(s); 
    if isempty(line), continue; end
    line = strtrim(string(line));
    

    if isnan(str2double(line)) % print any strings that the microcontroller may send (mostly testing)
        disp("MCU Output: " + line);
    end

    if contains(line, "SESSION_START") % handles the session start, outputs how many layers are in memory (0 at this point)
        fprintf("Captured layers in memory: %d\n", length(all_scans));
        
    elseif contains(line, "SCAN_START")
        scan_buf = []; % clear for new scan
        inScan = true;
        fprintf("Receiving data for %d...\n", length(all_scans)+1);

    elseif contains(line, "SCAN_CANCELLED") % handles errors from hardware (cancelled due to missing measurement etc.)
        inScan = false;
        scan_buf = [];
        disp("Transfer stopped by hardware.");

    elseif contains(line, "SCAN_END") && inScan % handles end of session (stores layers)
        if ~isempty(scan_buf)
            all_scans{end+1} = scan_buf;
            fprintf("Success! Layer %d stored.\n", length(all_scans));
        end
        inScan = false;

    else
        val = str2double(line); % numeric data only added to scan buffer
        if ~isnan(val) && inScan
            scan_buf(end+1) = val;
        end
    end
end

clear s;
if ishandle(fig), close(fig); end % close and clean up button and serial

if isempty(all_scans), disp("No data."); return; end % checks for no data error

fprintf("\nCalculating cords for %d layers...\n", length(all_scans)); 
angles = linspace(0, 360, NUM_POINTS + 1);
angles(end) = [];
angles = deg2rad(angles + offfset); % convert to radians, apply offset, overall preperation of angle of polar coordinate

% initialize 3D coordinates in Cartesian planes (after conversion from polar)
x = cell(length(all_scans), 1);
y = cell(length(all_scans), 1);
z = cell(length(all_scans), 1);
x_total = []; y_total = []; z_total = [];

for i = 1:length(all_scans) % for loop to go through all scans
    scan = all_scans{i};
    x_pos = (i-1) * STEP_MM; % x position is just a given 30 cm increment
    xs = []; ys = []; zs = [];
    
    for j = 1:length(scan) % for loop to go through each scan
        d = scan(j);
        if d < 20 || d > 4000, continue; end
        
        a = angles(mod(j-1, NUM_POINTS) + 1);
        ys(end+1) = d * cos(a); % calculate y and z from unit circle math
        zs(end+1) = d * sin(a);
        xs(end+1) = x_pos;
        
        x_total(end+1) = x_pos;
        y_total(end+1) = d * cos(a);
        z_total(end+1) = d * sin(a);
    end
    
    x{i} = xs; y{i} = ys; z{i} = zs;
end


% PLOT
figure('Color', 'w', 'Name', '3d Map'); hold on;
num_scans = length(all_scans);
colors = turbo(max(num_scans, 2)); % different coloring for layers for easy visuals

for i = 1:num_scans
    xs = x{i}; ys = y{i}; zs = z{i};
    if isempty(xs), continue; end
    c = colors(i, :);

    scatter3(xs, ys, zs, 35, c, 'filled');
    plot3([xs, xs(1)], [ys, ys(1)], [zs, zs(1)], '-', 'Color', c, 'LineWidth', 1.5);
    
    if i > 1
        pxs = x{i-1}; pys = y{i-1}; pzs = z{i-1};
        n = min(length(xs), length(pxs));
        for k = 1:n
            plot3([pxs(k) xs(k)], [pys(k) ys(k)], [pzs(k) zs(k)], ':', 'Color', [0.5 0.5 0.5 0.4]);
        end
    end
end

axis equal; grid on; view(3); rotate3d on; colormap turbo;
xlabel('X (mm)'); ylabel('Y (mm)'); zlabel('Z (mm)'); % labelling
title(sprintf('Final 3D Graph: %d Vertices', length(x_total)));

%center graph and scale appropriately
if ~isempty(x_total)
    x_mid = (max(x_total) + min(x_total)) / 2;
    y_mid = (max(y_total) + min(y_total)) / 2;
    z_mid = (max(z_total) + min(z_total)) / 2;
    range = max([max(x_total) - min(x_total), max(y_total) - min(y_total), max(z_total) - min(z_total)]) / 2;
    if range < 100, range = 100; end
    xlim([x_mid-range, x_mid+range]);
    ylim([y_mid - range, y_mid + range]);
    zlim([z_mid - range, z_mid + range]);
end

fprintf("Plotting complete.\n"); % finished