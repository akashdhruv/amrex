//
// $Id: ParallelDescriptor.cpp,v 1.76 2001-07-20 04:46:21 car Exp $
//

#include <Utility.H>
#include <ParallelDescriptor.H>
#include <ParmParse.H>

//
// Definition of non-inline members of CommData.
//

int
ParallelDescriptor::MyProc ()
{
    BL_ASSERT(m_MyId != -1);
    return m_MyId;
}

int
ParallelDescriptor::NProcs ()
{
    BL_ASSERT(m_nProcs != -1);
    return m_nProcs;
}

int
ParallelDescriptor::NProcsCFD ()
{
    if (m_nProcsCFD == -1)
        SetNProcsCFD();

    BL_ASSERT(m_nProcsCFD != -1);

    return m_nProcsCFD;
}

bool
ParallelDescriptor::IOProcessor ()
{
    BL_ASSERT(m_MyId != -1);
    return m_MyId == ioProcessor;
}

int
ParallelDescriptor::IOProcessorNumber ()
{
    return ioProcessor;
}

MPI_Comm
ParallelDescriptor::Communicator ()
{
    return m_comm;
}

CommData::CommData ()
{
    for (int i = 0; i < length(); i++)
        m_data[i] = 0;
}

CommData::CommData (int        face,
                    int        fabindex,
                    int        fromproc,
                    int        id,
                    int        ncomp,
                    int        srccomp,
                    int        fabarrayid,
                    const Box& box)
{
    m_data[0] = face;
    m_data[1] = fabindex;
    m_data[2] = fromproc;
    m_data[3] = id;
    m_data[4] = ncomp;
    m_data[5] = srccomp;
    m_data[6] = fabarrayid;

    const IntVect& sm = box.smallEnd();

    for (int i = 0; i < BL_SPACEDIM; i++)
        m_data[7+i] = sm[i];

    const IntVect& bg = box.bigEnd();

    for (int i = 0; i < BL_SPACEDIM; i++)
        m_data[7+BL_SPACEDIM+i] = bg[i];

    IntVect typ = box.type();

    for (int i = 0; i < BL_SPACEDIM; i++)
        m_data[7+2*BL_SPACEDIM+i] = typ[i];
}

CommData::CommData (const CommData& rhs)
{
    for (int i = 0; i < length(); i++)
        m_data[i] = rhs.m_data[i];
}

CommData&
CommData::operator= (const CommData& rhs)
{
    if (!(this == &rhs))
    {
        for (int i = 0; i < length(); i++)
            m_data[i] = rhs.m_data[i];
    }
    return *this;
}

bool
CommData::operator== (const CommData& rhs) const
{
    for (int i = 0; i < length(); i++)
        if (!(m_data[i] == rhs.m_data[i]))
            return false;

    return true;
}

std::ostream&
operator<< (std::ostream&   os,
            const CommData& cd)
{
    os << cd.face()       << ' '
       << cd.fabindex()   << ' '
       << cd.fromproc()   << ' '
       << cd.id()         << ' '
       << cd.nComp()      << ' '
       << cd.srcComp()    << ' '
       << cd.fabarrayid() << ' '
       << cd.box();

    return os;
}

std::ostream&
operator<< (std::ostream&          os,
            const Array<CommData>& cd)
{
    for (int i = 0; i < cd.length(); i++)
        os << cd[i] << '\n';
    return os;
}

#ifdef BL_USE_MPI

#include <ccse-mpi.H>

static const aString REDUCE("mpi_reduce");

int ParallelDescriptor::m_nProcs    = -1;
int ParallelDescriptor::m_nProcsCFD = -1;
int ParallelDescriptor::m_MyId      = -1;
MPI_Comm ParallelDescriptor::m_comm = MPI_COMM_WORLD;

void
ParallelDescriptor::Abort ()
{
#ifdef WIN32
    // if ( m_nProcs <= 1 ) throw;
    throw;
#endif
    MPI_Abort(Communicator(), -1);
}

void
ParallelDescriptor::Abort (int errorcode)
{
    BoxLib::Abort(ErrorString(errorcode));
}

const char* ParallelDescriptor::ErrorString (int errorcode)
{
    BL_ASSERT(errorcode > 0 && errorcode <= MPI_ERR_LASTCODE);

    int len = 0;

    static char msg[MPI_MAX_ERROR_STRING+1];

    MPI_Error_string(errorcode, msg, &len);

    BL_ASSERT(len <= MPI_MAX_ERROR_STRING);

    return msg;
}

void
ParallelDescriptor::SetNProcsCFD ()
{
    BL_ASSERT(m_nProcs != -1);
    BL_ASSERT(m_nProcsCFD == -1);

    m_nProcsCFD = m_nProcs;

    ParmParse pp("ParallelDescriptor");

    if (pp.query("nProcsCFD",m_nProcsCFD))
    {
        if (!(m_nProcsCFD > 0 && m_nProcsCFD <= m_nProcs))
            m_nProcsCFD = m_nProcs;

        if (ParallelDescriptor::IOProcessor())
            std::cout << "--> Running job with NProcsCFD = "
                      << m_nProcsCFD
                      << std::endl;
    }
}

void
ParallelDescriptor::StartParallel (int*    argc,
                                   char*** argv)
{
    BL_ASSERT(m_MyId == -1);
    BL_ASSERT(m_nProcs == -1);
    BL_ASSERT(m_nProcsCFD == -1);

    int rc, sflag;

    if ((rc = MPI_Initialized(&sflag)) != MPI_SUCCESS)
	ParallelDescriptor::Abort(rc);

    if (!sflag)
	if ((rc = MPI_Init(argc, argv)) != MPI_SUCCESS)
            ParallelDescriptor::Abort(rc);
    
    if ((rc = MPI_Comm_size(Communicator(), &m_nProcs)) != MPI_SUCCESS)
        ParallelDescriptor::Abort(rc);

    if ((rc = MPI_Comm_rank(Communicator(), &m_MyId)) != MPI_SUCCESS)
        ParallelDescriptor::Abort(rc);
    //
    // Wait till all other processes are properly started.
    //
    if ((rc = MPI_Barrier(Communicator())) != MPI_SUCCESS)
        ParallelDescriptor::Abort(rc);
}

void
ParallelDescriptor::EndParallel ()
{
    BL_ASSERT(m_MyId != -1);
    BL_ASSERT(m_nProcs != -1);

    int rc = MPI_Finalize();

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
}

double
ParallelDescriptor::second ()
{
    return MPI_Wtime();
}

void
ParallelDescriptor::Barrier ()
{
    int rc = MPI_Barrier(Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
}

void
ParallelDescriptor::DoAllReduceReal (Real& r,
                                     MPI_Op   op)
{
    Real recv;

    int rc = MPI_Allreduce(&r,
                           &recv,
                           1,
                           mpi_data_type(&recv),
                           op,
                           Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
    
    r = recv;
}

void
ParallelDescriptor::DoReduceReal (Real& r,
                                  MPI_Op   op,
                                  int   cpu)
{
    Real recv;

    int rc = MPI_Reduce(&r,
                        &recv,
                        1,
                        mpi_data_type(&recv),
                        op,
                        cpu,
                        Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);

    if (ParallelDescriptor::MyProc() == cpu)
        r = recv;
}

void
ParallelDescriptor::ReduceRealMax (Real& r)
{
    DoAllReduceReal(r,MPI_MAX);
}

void
ParallelDescriptor::ReduceRealMin (Real& r)
{
    DoAllReduceReal(r,MPI_MIN);
}

void
ParallelDescriptor::ReduceRealSum (Real& r)
{
    DoAllReduceReal(r,MPI_SUM);
}

void
ParallelDescriptor::ReduceRealMax (Real& r, int cpu)
{
    DoReduceReal(r,MPI_MAX,cpu);
}

void
ParallelDescriptor::ReduceRealMin (Real& r, int cpu)
{
    DoReduceReal(r,MPI_MIN,cpu);
}

void
ParallelDescriptor::ReduceRealSum (Real& r, int cpu)
{
    DoReduceReal(r,MPI_SUM,cpu);
}

void
ParallelDescriptor::DoAllReduceLong (long& r,
                                     MPI_Op   op)
{
    long recv;

    int rc = MPI_Allreduce(&r,
                           &recv,
                           1,
                           MPI_LONG,
                           op,
                           Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
    
    r = recv;
}

void
ParallelDescriptor::DoReduceLong (long& r,
                                  MPI_Op   op,
                                  int   cpu)
{
    long recv;

    int rc = MPI_Reduce(&r,
                        &recv,
                        1,
                        MPI_LONG,
                        op,
                        cpu,
                        Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);

    if (ParallelDescriptor::MyProc() == cpu)
        r = recv;
}

void
ParallelDescriptor::ReduceLongAnd (long& r)
{
    DoAllReduceLong(r,MPI_LAND);
}

void
ParallelDescriptor::ReduceLongSum (long& r)
{
    DoAllReduceLong(r,MPI_SUM);
}

void
ParallelDescriptor::ReduceLongMax (long& r)
{
    DoAllReduceLong(r,MPI_MAX);
}

void
ParallelDescriptor::ReduceLongMin (long& r)
{
    DoAllReduceLong(r,MPI_MIN);
}

void
ParallelDescriptor::ReduceLongAnd (long& r, int cpu)
{
    DoReduceLong(r,MPI_LAND,cpu);
}

void
ParallelDescriptor::ReduceLongSum (long& r, int cpu)
{
    DoReduceLong(r,MPI_SUM,cpu);
}

void
ParallelDescriptor::ReduceLongMax (long& r, int cpu)
{
    DoReduceLong(r,MPI_MAX,cpu);
}

void
ParallelDescriptor::ReduceLongMin (long& r, int cpu)
{
    DoReduceLong(r,MPI_MIN,cpu);
}

void
ParallelDescriptor::DoAllReduceInt (int& r,
                                    MPI_Op  op)
{
    int recv;

    int rc = MPI_Allreduce(&r,
                           &recv,
                           1,
                           MPI_INT,
                           op,
                           Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
    
    r = recv;
}

void
ParallelDescriptor::DoReduceInt (int& r,
                                 MPI_Op  op,
                                 int  cpu)
{
    int recv;

    int rc = MPI_Reduce(&r,
                        &recv,
                        1,
                        MPI_INT,
                        op,
                        cpu,
                        Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);

    if (ParallelDescriptor::MyProc() == cpu)
        r = recv;
}

void
ParallelDescriptor::ReduceIntSum (int& r)
{
    DoAllReduceInt(r,MPI_SUM);
}

void
ParallelDescriptor::ReduceIntMax (int& r)
{
    DoAllReduceInt(r,MPI_MAX);
}

void
ParallelDescriptor::ReduceIntMin (int& r)
{
    DoAllReduceInt(r,MPI_MIN);
}

void
ParallelDescriptor::ReduceIntSum (int& r, int cpu)
{
    DoReduceInt(r,MPI_SUM,cpu);
}

void
ParallelDescriptor::ReduceIntMax (int& r, int cpu)
{
    DoReduceInt(r,MPI_MAX,cpu);
}

void
ParallelDescriptor::ReduceIntMin (int& r, int cpu)
{
    DoReduceInt(r,MPI_MIN,cpu);
}

void
ParallelDescriptor::ReduceBoolAnd (bool& r)
{
    int src = r; // `src' is either 0 or 1.

    DoAllReduceInt(src,MPI_SUM);

    r = (src == ParallelDescriptor::NProcs()) ? true : false;
}

void
ParallelDescriptor::ReduceBoolOr (bool& r)
{
    int src = r; // `src' is either 0 or 1.

    DoAllReduceInt(src,MPI_SUM);

    r = (src == 0) ? false : true;
}

void
ParallelDescriptor::ReduceBoolAnd (bool& r, int cpu)
{
    int src = r; // `src' is either 0 or 1.

    DoReduceInt(src,MPI_SUM,cpu);

    if (ParallelDescriptor::MyProc() == cpu)
        r = (src == ParallelDescriptor::NProcs()) ? true : false;
}

void
ParallelDescriptor::ReduceBoolOr (bool& r, int cpu)
{
    int src = r; // `src' is either 0 or 1.

    DoReduceInt(src,MPI_SUM,cpu);

    if (ParallelDescriptor::MyProc() == cpu)
        r = (src == 0) ? false : true;
}

void
ParallelDescriptor::Broadcast (int   fromproc,
                               void* src,
                               void* dest,
                               int   nbytes)
{
    BL_ASSERT(src == dest);

    MPI_Bcast(src, nbytes, MPI_BYTE, fromproc, Communicator());
}

void
ParallelDescriptor::Gather (Real* sendbuf,
                            int   nsend,
                            Real* recvbuf,
                            int   root)
{
    BL_ASSERT(root >= 0);
    BL_ASSERT(nsend > 0);
    BL_ASSERT(!(sendbuf == 0));
    BL_ASSERT(!(recvbuf == 0));

    MPI_Datatype typ = mpi_data_type(sendbuf);

    int rc = MPI_Gather(sendbuf,
                        nsend,
                        typ,
                        recvbuf,
                        nsend,
                        typ,
                        root,
                        Communicator());

    if (!(rc == MPI_SUCCESS))
        ParallelDescriptor::Abort(rc);
}

#else

int ParallelDescriptor::m_nProcs    = 1;
int ParallelDescriptor::m_nProcsCFD = 1;
int ParallelDescriptor::m_MyId      = 0;
MPI_Comm ParallelDescriptor::m_comm = 0;

void
ParallelDescriptor::Gather (Real* sendbuf,
			    int   nsend,
			    Real* recvbuf,
			    int   root)
{
    BL_ASSERT(root == 0);
    BL_ASSERT(nsend > 0);
    BL_ASSERT(!(sendbuf == 0));
    BL_ASSERT(!(recvbuf == 0));

    for (int i = 0; i < nsend; ++i)
        recvbuf[i] = sendbuf[i];
}

void ParallelDescriptor::SetNProcsCFD () {}

void ParallelDescriptor::StartParallel (int*, char***) {}
void ParallelDescriptor::EndParallel () {}

void ParallelDescriptor::Abort () { ::abort(); }
void ParallelDescriptor::Abort (int) { ::abort(); }
const char* ParallelDescriptor::ErrorString (int) { return ""; }
void ParallelDescriptor::Barrier () {}

void ParallelDescriptor::ReduceRealMax (Real&) {}
void ParallelDescriptor::ReduceRealMin (Real&) {}
void ParallelDescriptor::ReduceRealSum (Real&) {}

void ParallelDescriptor::ReduceRealMax (Real&,int) {}
void ParallelDescriptor::ReduceRealMin (Real&,int) {}
void ParallelDescriptor::ReduceRealSum (Real&,int) {}

void ParallelDescriptor::ReduceLongAnd (long&) {}
void ParallelDescriptor::ReduceLongSum (long&) {}
void ParallelDescriptor::ReduceLongMax (long&) {}
void ParallelDescriptor::ReduceLongMin (long&) {}

void ParallelDescriptor::ReduceLongAnd (long&,int) {}
void ParallelDescriptor::ReduceLongSum (long&,int) {}
void ParallelDescriptor::ReduceLongMax (long&,int) {}
void ParallelDescriptor::ReduceLongMin (long&,int) {}

void ParallelDescriptor::ReduceIntSum (int&) {}
void ParallelDescriptor::ReduceIntMax (int&) {}
void ParallelDescriptor::ReduceIntMin (int&) {}

void ParallelDescriptor::ReduceIntSum (int&,int) {}
void ParallelDescriptor::ReduceIntMax (int&,int) {}
void ParallelDescriptor::ReduceIntMin (int&,int) {}

void ParallelDescriptor::ReduceBoolAnd (bool&) {}
void ParallelDescriptor::ReduceBoolOr  (bool&) {}

void ParallelDescriptor::ReduceBoolAnd (bool&,int) {}
void ParallelDescriptor::ReduceBoolOr  (bool&,int) {}

void ParallelDescriptor::Broadcast (int,void*,void*,int) {}
//
// Here so we don't need to include <Utility.H> in <ParallelDescriptor.H>.
//
double
ParallelDescriptor::second ()
{
    return BoxLib::second();
}

#endif
//
// This function is the same whether or not we're using MPI.
//
int
ParallelDescriptor::SeqNum ()
{
    const int BEG = 1000;
    const int END = 9000;

    static int seqno = BEG;

    int result = seqno++;

    if (seqno > END) seqno = BEG;

    return result;
}

#if  defined(BL_FORT_USE_UPPERCASE)
#define FORT_BL_PD_BARRIER 	BL_PD_BARRIER
#define FORT_BL_PD_COMMUNICATOR BL_PD_COMMUNICATOR
#define FORT_BL_PD_MYPROC 	BL_PD_MYPROC
#define FORT_BL_PD_NPROCS 	BL_PD_NPROCS
#define FORT_BL_PD_IOPROC 	BL_PD_IOPROC
#define FORT_BL_PD_ABORT  	BL_PD_ABORT
#elif defined(BL_FORT_USE_LOWERCASE)
#define FORT_BL_PD_BARRIER 	bl_pd_barrier
#define FORT_BL_PD_COMMUNICATOR bl_pd_communicator
#define FORT_BL_PD_MYPROC 	bl_pd_myproc
#define FORT_BL_PD_NPROCS 	bl_pd_nprocs
#define FORT_BL_PD_IOPROC 	bl_pd_ioproc
#define FORT_BL_PD_ABORT  	bl_pd_abort
#elif defined(BL_FORT_USE_UNDERSCORE)
#define FORT_BL_PD_BARRIER 	bl_pd_barrier_
#define FORT_BL_PD_COMMUNICATOR bl_pd_communicator_
#define FORT_BL_PD_MYPROC 	bl_pd_myproc_
#define FORT_BL_PD_NPROCS 	bl_pd_nprocs_
#define FORT_BL_PD_IOPROC 	bl_pd_ioproc_
#define FORT_BL_PD_ABORT  	bl_pd_abort_
#endif

extern  "C"
void
FORT_BL_PD_BARRIER ()
{
    ParallelDescriptor::Barrier();
}

extern  "C"
void
FORT_BL_PD_COMMUNICATOR (void* vcomm)
{
    MPI_Comm* comm = reinterpret_cast<MPI_Comm*>(vcomm);

    *comm = ParallelDescriptor::Communicator();
}

extern  "C"
void
FORT_BL_PD_MYPROC (int* myproc)
{
    *myproc = ParallelDescriptor::MyProc();
}

extern  "C"
void
FORT_BL_PD_NPROCS (int* nprocs)
{
    *nprocs = ParallelDescriptor::NProcs();
}

extern  "C"
void
FORT_BL_PD_IOPROC (int* ioproc)
{
    *ioproc = ParallelDescriptor::IOProcessorNumber();
}

extern  "C"
void
FORT_BL_PD_ABORT ()
{
    ParallelDescriptor::Abort();
}

