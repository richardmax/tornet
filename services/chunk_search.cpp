#include "chunk_search.hpp"

#include <tornet/rpc/client.hpp>
#include <tornet/services/reflect_chunk_session.hpp>
#include <scrypt/bigint.hpp>

namespace tornet {

chunk_search::chunk_search( const node::ptr& local_node, const scrypt::sha1& target, 
                            uint32_t N, uint32_t P, bool fn )
:kad_search( local_node, target, N, P ), find_nm(fn), avg_qr(0), qr_weight(0), deadend_count(0) {

}

bool chunk_search::filter( const node::id_type& id ) {
   tornet::channel                      chan = get_node()->open_channel( id, 101 );
   tornet::rpc::connection::ptr         con( new tornet::rpc::connection(chan));
   tornet::rpc::client<chunk_session>   chunk_client(con);
   fetch_response fr = chunk_client->fetch( target(), 0, 0 );

   // update avg query rate... the closer a node is to the target the more 
   // often that node should be queried and the more accurate its estimate of
   // propularity should be.
   scrypt::bigint d( (char*)(target() ^ id).hash, sizeof( id.hash ) );

   deadend_count += fr.deadend_count;

   // convert interval into hz
   double query_rate_hz = 1000000 / double(fr.query_interval);
   int level = std::min( int(16- (160-d.log2()) ), int(1) ); 
   // a far away node should receive only say 2% of total queries, so we must
   // multiply by 50 to get the implied/normalized global rate
   double adj_rate =  level * query_rate_hz;
   // but because it is so far away, there is more error so its significance
   // is less than closer nodes.
   double weight = 1.0 / (1<<(level-1));

   // calculate the new running weighted average query rate
   avg_qr = (avg_qr * qr_weight + adj_rate) / (qr_weight + weight);
   qr_weight = qr_weight + weight;

   if( fr.result == chunk_session_result::available ) {
      return true;
   } else {
     m_near_results[id^target()] = id; 
     if( m_n && m_near_results.size() > m_n ) {
        m_near_results.erase( --m_near_results.end() );
     }
   }
   return false;
}

double   chunk_search::avg_query_rate()const    { return avg_qr;        }
double   chunk_search::query_rate_weight()const { return qr_weight;     }
uint32_t chunk_search::get_deadend_count()const { return deadend_count; }



} // namespace tornet
