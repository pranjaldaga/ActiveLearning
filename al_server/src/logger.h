#if !defined(_LOGGER_H_)
#define _LOGGER_H_

#include <fstream>
#include <pthread.h>



class EvtLogger
{
public:
	enum LogType {Evt_ERROR, Evt_INFO, Evt_WARN};

			EvtLogger(std::string logFile);
			~EvtLogger();

	bool	LogMsg(LogType type, std::string msg);
	bool	LogMsgv(LogType type, const char *msg, ...);
	double 	WallTime(void);



protected:

	std::ofstream	m_logFile;
	std::string		m_fqfn;
	pthread_mutex_t	m_fileMtx;
	int				m_curFileDay;

	void 	Archive(void);
	
};


#endif
