#include <Shared/Shrink.h>
#include "Context.h"
class BufferedSocket **sockets;
int numSockets;

#define HS_NEED_SEND  1
#define HS_NEED_REC   2
#define HS_NEED_INIT  4

class BufferedSocket
{
public:
	SOCKET sock;
	unsigned char handshakeState;

	char *outBuffer;
	int outBufferLen;

	char *inBuffer;
	int inBufferLen;

	int headerLen;

	ContextProcessInfo *process;

	BufferedSocket(SOCKET s)
	{
		handshakeState = HS_NEED_SEND | HS_NEED_REC | HS_NEED_INIT;
		sock = s;
		outBuffer = 0;
		outBufferLen = 0;
		inBuffer = 0;
		inBufferLen = 0;
		process = 0;
		headerLen = sizeof(wchar_t) * (1 + wcslen(MAIN_WINDOW_TITLE));
		sockets = (BufferedSocket**) realloc(sockets, sizeof(BufferedSocket*) * (numSockets+1));
		sockets[numSockets++] = this;
	}

	~BufferedSocket()
	{
		if (process)
			process->ReleaseSocket();
		int i=0;
		for (; i<numSockets; i++)
			if (sockets[i] == this)
				break;
		if (i == numSockets) return;
		numSockets --;
		for (; i<numSockets; i++)
			sockets[i] = sockets[i+1];
		if (!numSockets)
		{
			free(sockets);
			sockets = 0;
		}
		closesocket(sock);
	}

	void Connected()
	{
		handshakeState &= ~HS_NEED_SEND;
		QueueData(MAIN_WINDOW_TITLE, headerLen);
	}

	void ReadData()
	{
		static int count = 0;
		while (1)
		{
			inBuffer = (char*) realloc(inBuffer, inBufferLen + 4096);
			int d = recv(sock, inBuffer+inBufferLen, 4096, 0);
			if (d == SOCKET_ERROR || d < 0)
			{
				if (GetLastError() == WSAEWOULDBLOCK)
					return;
				delete this;
				return;
			}
			if (!d) return;
			inBufferLen += d;
			if (handshakeState & HS_NEED_REC)
			{
				if (inBufferLen < 4)
					return;
				int len = *(int*)inBuffer;
				if (len != headerLen)
				{
					delete this;
					return;
				}
				if (inBufferLen < len +4)
				{
					if (memcmp(inBuffer+4, MAIN_WINDOW_TITLE, inBufferLen-4))
						delete this;
					return;
				}
				if (memcmp(inBuffer+4, MAIN_WINDOW_TITLE, headerLen))
				{
					delete this;
					return;
				}
				memmove(inBuffer, inBuffer+4+len, inBufferLen -= 4+len);
				handshakeState &= ~HS_NEED_REC;
			}
			while (1)
			{
				if (inBufferLen < 4)
					return;
				if (inBufferLen < 0)
				{
					delete this;
					return;
				}
				int len = *(int*)inBuffer;
				if (len + 4 > inBufferLen)
					return;
				wchar_t *string = (wchar_t*)(inBuffer+4);
				if (string[len/2-1])
				{
					delete this;
					return;
				}
				if (handshakeState & HS_NEED_INIT)
				{
					InjectionSettings settings;
					wchar_t *exeName;
					wchar_t *pidString;
					if (!string || !(exeName = wcsrchr(string, '\\')) || !(pidString = wcsrchr(exeName, '*')))
					{
						delete this;
						return;
					}
					*pidString = 0;
					pidString++;
					wchar_t *exeNamePlusFolder = exeName;
					while (exeNamePlusFolder > string && exeNamePlusFolder[-1] !='\\' && exeNamePlusFolder[-1] !=':')
						exeNamePlusFolder--;
					//exeName++;
					wchar_t temp[2*MAX_PATH];
					wchar_t *r;
					std::wstring internalHooks;
					if (!LoadInjectionSettings(settings, exeNamePlusFolder, 0, &internalHooks) ||
						!GetModuleFileName(0, temp, MAX_PATH) ||
						!(r = wcsrchr(temp, '\\')))
						{
							delete this;
							return;
					}

					process = Contexts.FindProcess(exeNamePlusFolder, _wtoi(pidString), &settings);

					wsprintfW(r+1, L"Rule Sets\\%s", settings.atlasConfig.trsPath);
					// If it's too long, just give up.
					if (wcslen(temp) >= MAX_PATH)
					{
						delete this;
						return;
					}
					wcscpy(settings.atlasConfig.trsPath, temp);
					if (!QueueData(&settings, sizeof(settings)))
						return;
					if (!QueueData(internalHooks.c_str(), sizeof(wchar_t)*(1+internalHooks.length())))
						return;

					process->AddSocket();
					handshakeState &= ~HS_NEED_INIT;
				}
				else
				{
					count ++;
					wchar_t *threadContext = string;
					wchar_t *text = wcschr(string, '\n');
					if (text)
					{
						*text = 0;
						text++;
						Contexts.AddText(process, threadContext, text);
					}
					// Add to context.
				}
				memmove(inBuffer, inBuffer+4+len, inBufferLen -= 4+len);
			}
		}
	}

	int QueueData(const void* data, int len)
	{
		if (handshakeState & HS_NEED_SEND)
			Connected();
		int add = sizeof(len) + len;
		outBuffer = (char*)realloc(outBuffer, outBufferLen + add);
		*(int*)(outBuffer+outBufferLen) = len;
		outBufferLen += sizeof(len);
		memcpy(outBuffer+outBufferLen, data, len);
		outBufferLen += len;
		if (outBufferLen == add)
			return SendData();
		return 1;
	}

	int SendData()
	{
		if (handshakeState & HS_NEED_SEND)
			Connected();
		int sent = 0;
		while (sent != outBufferLen)
		{
			int s = send(sock, outBuffer + sent, outBufferLen, 0);
			if (s == SOCKET_ERROR || s < 0)
			{
				if (GetLastError() != WSAEWOULDBLOCK)
				{
					delete this;
					return 0;
				}
				break;
			}
			sent += s;
		}
		if (!outBufferLen)
		{
			free(outBuffer);
			outBuffer = 0;
		}
		else
			memmove(outBuffer, outBuffer+sent, outBufferLen -= sent);
		return 1;
	}
};

HANDLE hMapping = 0;
SOCKET listenSocket = INVALID_SOCKET;

void StopListen()
{
	if (listenSocket != INVALID_SOCKET)
	{
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}
	if (hMapping)
	{
		CloseHandle(hMapping);
		hMapping = 0;
	}
}

void CleanupSockets()
{
	StopListen();
	while (numSockets)
		delete sockets[numSockets-1];
}

int StartListen()
{
	StopListen();
	unsigned short port;
	for (int i=0; i<20; i++)
	{
		listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSocket == INVALID_SOCKET)
			continue;
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		port = config.port + i;
		addr.sin_port = htons(port);
		addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
		if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)))
		{
			int w = GetLastError();
			StopListen();
			continue;
		}

		if (listen(listenSocket, SOMAXCONN))
		{
			StopListen();
			continue;
		}

		if (WSAAsyncSelect(listenSocket, hWndSuperMaster, WMA_SOCKET_MESSAGE, FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE | FD_CONNECT))
		{
			StopListen();
			continue;
		}
		break;
	}
	if (listenSocket == INVALID_SOCKET)
		return 0;
	if (!hMapping)
	{
		hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, 2, MAIN_WINDOW_TITLE);
		if (!hMapping || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			StopListen();
			return 0;
		}
	}
	void *v = MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 2);
	if (!v)
	{
		StopListen();
		return 0;
	}
	*(unsigned short*)v = port;
	UnmapViewOfFile(v);
	return 1;
}

void TryStartListen(int forceRestart)
{
	if (listenSocket != INVALID_SOCKET && !forceRestart) return;
	if (StartListen()) return;
	MessageBox(0, L"Unable to create socket.  Hooking engine won't work.", L"Error", MB_ICONERROR | MB_OK);
}

int CALLBACK ScreenIP(IN LPWSABUF lpCallerId, IN LPWSABUF lpCallerData, IN OUT LPQOS lpSQOS, IN OUT LPQOS lpGQOS, IN LPWSABUF lpCalleeId, OUT LPWSABUF lpCalleeData, OUT GROUP FAR *g, IN DWORD_PTR dwCallbackData)
{
	if (!lpCallerId || lpCallerId->len < sizeof(sockaddr_in) ||
		((sockaddr_in*)lpCallerId->buf)->sin_addr.S_un.S_addr != inet_addr("127.0.0.1"))
			return CF_REJECT;
	return CF_ACCEPT;
}

void HandleSocketMessage(SOCKET sock, unsigned int msg, unsigned int error)
{
	if (sock == listenSocket)
	{
		if (error || msg == FD_CLOSE)
			TryStartListen(1);
		else
		{
			sock = WSAAccept(listenSocket, 0, 0, ScreenIP, 0);
			if (listenSocket != INVALID_SOCKET)
				new BufferedSocket(sock);
		}
		return;
	}
	BufferedSocket *s = 0;
	for (int i=0; i<numSockets; i++)
	{
		if (sock == sockets[i]->sock)
		{
			s = sockets[i];
			break;
		}
	}
	if (!s) return;

	if (error || msg == FD_CLOSE)
		delete s;
	// Doesn't seem to be called
	/*else if (msg == FD_CONNECT) {
		s->Connected();
	}//*/
	else if (msg == FD_READ)
		s->ReadData();
	else if (msg == FD_WRITE)
		s->SendData();
}
