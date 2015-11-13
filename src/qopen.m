function [k] = qopen(varargin)
    %
    % Create matlab structure for connection to KDB/Q server
    %
    port = 5000;
    host = 'localhost';
    if nargin >= 1, port = varargin{1}; end
    if nargin >= 2, host = varargin{1}; port = varargin{2}; end
	k = struct('host', host, 'port', port);
end