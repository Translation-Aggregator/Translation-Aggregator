void HandleSocketMessage(SOCKET sock, unsigned int msg, unsigned int error);
void TryStartListen(int forceRestart);
void StopListen();
void CleanupSockets();
