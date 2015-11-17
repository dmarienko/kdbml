function [k] = qopen(varargin)
    %
    % Create matlab structure for connection to KDB/Q server
    %
    port = 5000;
    host = 'localhost';
    if nargin >= 1, port = varargin{1}; end
    if nargin >= 2, host = varargin{1}; port = varargin{2}; end
	hs = struct('host', host, 'port', port);

    % return query lamba
    function r = Q(q), r = qdbc(hs, q); end
    k = @Q;
end