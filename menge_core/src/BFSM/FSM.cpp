/*

License

Menge
Copyright � and trademark � 2012-14 University of North Carolina at Chapel Hill. 
All rights reserved.

Permission to use, copy, modify, and distribute this software and its documentation 
for educational, research, and non-profit purposes, without fee, and without a 
written agreement is hereby granted, provided that the above copyright notice, 
this paragraph, and the following four paragraphs appear in all copies.

This software program and documentation are copyrighted by the University of North 
Carolina at Chapel Hill. The software program and documentation are supplied "as is," 
without any accompanying services from the University of North Carolina at Chapel 
Hill or the authors. The University of North Carolina at Chapel Hill and the 
authors do not warrant that the operation of the program will be uninterrupted 
or error-free. The end-user understands that the program was developed for research 
purposes and is advised not to rely exclusively on the program for any reason.

IN NO EVENT SHALL THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL OR THE AUTHORS 
BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
DAMAGES, INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS 
DOCUMENTATION, EVEN IF THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL OR THE 
AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL AND THE AUTHORS SPECIFICALLY 
DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND ANY STATUTORY WARRANTY 
OF NON-INFRINGEMENT. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND 
THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL AND THE AUTHORS HAVE NO OBLIGATIONS 
TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

Any questions or comments should be sent to the authors {menge,geom}@cs.unc.edu

*/

#include "FSM.h"

#include "State.h"
#include "Transitions/Transition.h"
#include "Tasks/Task.h"
#include "Core.h"
#include "FsmContext.h"
#include "StateContext.h"
#include "GoalSet.h"
#include "Events/EventSystem.h"

#include "BaseAgent.h"
#include "SimulatorInterface.h"

namespace Menge {

	namespace BFSM {

		/////////////////////////////////////////////////////////////////////
		//                   Implementation of FSM
		/////////////////////////////////////////////////////////////////////

		FSM::FSM( Agents::SimulatorInterface * sim ):_sim(sim), _agtCount(0), _currNode(0x0) {	
			setAgentCount( sim->getNumAgents() );
		}

		/////////////////////////////////////////////////////////////////////

		FSM::~FSM() {
			if ( _currNode ) {
				delete [] _currNode;
			}
			// TODO: Why doesn't this delete the States and transitions?
			std::map< size_t, GoalSet * >::iterator gsItr = _goalSets.begin();
			for ( ; gsItr != _goalSets.end(); ++gsItr ) {
				delete gsItr->second;
			}
			for ( size_t i = 0; i < _tasks.size(); ++i ) {
				_tasks[ i ]->destroy();
			}
			_tasks.clear();
		}

		/////////////////////////////////////////////////////////////////////
		 
		void FSM::collectTasks() {
			const size_t STATE_COUNT = _nodes.size();
			for ( size_t i = 0; i < STATE_COUNT; ++i ) {
				_nodes[ i ]->getTasks( this );
			}

			//now collect the velocity modifiers tasks
			std::vector< VelModifier * >::iterator vItr = _velModifiers.begin();
			for ( ; vItr != _velModifiers.end(); ++vItr ) {   //TODO: replace global vel mod initalizer
				addTask((*vItr)->getTask());
			}

			//iterate over agents

		}

		/////////////////////////////////////////////////////////////////////
		 
		void FSM::addTask( Task * task ) {
			if ( task ) {
				for ( size_t i = 0; i < _tasks.size(); ++i ) {
					if ( task->isEquivalent( _tasks[i] ) ) {
						task->destroy();
						return;
					}
				}
				_tasks.push_back( task );
			}
		}

		/////////////////////////////////////////////////////////////////////
		 
		void FSM::setAgentCount( size_t count ) {
			if ( _currNode ) {
				delete [] _currNode;
				_currNode = 0x0;
			}
			_agtCount = count;
			_currNode = new State * [ count ];
			memset( _currNode, 0x0, count * sizeof( State * ) );
		}

		/////////////////////////////////////////////////////////////////////
		 
		void FSM::advance( Agents::BaseAgent * agent ) {
			const size_t ID = agent->_id;
			// Evaluate the current state's transitions
			State * newNode = _currNode[ ID ]->testTransitions( agent );
			if ( newNode ) {
				_currNode[ ID ] = newNode;
			}
		}

		/////////////////////////////////////////////////////////////////////
		 
		void FSM::setPrefVelFromMsg( const geometry_msgs::Twist& msg){
			//copy from message and into PrefVelmsg
			ROS_INFO("I heard: x :[%f]", msg.linear.x);
   			ROS_INFO("I heard: y :[%f]", msg.linear.y);
   			ROS_INFO("I heard: z :[%f]", msg.linear.z);
   			ROS_INFO("I heard: x :[%f]", msg.angular.x);
   			ROS_INFO("I heard: y :[%f]", msg.angular.y);
   			ROS_INFO("I heard: z :[%f]", msg.angular.z);

			float speed = sqrt(pow(msg.linear.x,2) + pow(msg.linear.y,2));
			if(speed == 0) speed = 0.0001;

			prefVelMsg.setSpeed(speed);
			prefVelMsg.turn(msg.angular.z);
		}

		void FSM::computePrefVelocity( Agents::BaseAgent * agent ) {
			const size_t ID = agent->_id;
			// Evalute the new state's velocity

						
			//generate a preferred velocity for passing around
			Agents::PrefVelocity newVel;

			_currNode[ ID ]->getPrefVelocity( agent, newVel);

			//TODO: My velocity modifiers here
			
			

			std::vector< VelModifier * >::iterator vItr = _velModifiers.begin();
			for ( ; vItr != _velModifiers.end(); ++vItr ) {   //TODO: replace global vel mod initalizer
				(*vItr)->adaptPrefVelocity(agent, newVel);
			}
			if(agent->_isExternal){
				//std::cout << "External Agent detected : " << ID << std::endl;
				//prefVelMsg.setSpeed(0.0);
				ros::spinOnce();
				newVel = prefVelMsg;
				//std::cout << (newVel.getPreferred()).x() << " : " << (newVel.getPreferred()).y() << std::endl;
				//std::cout << "Direction Set from the ROS message!" << std::endl;
			}

			//agent will now have a set preferred velocity method
			agent->setPreferredVelocity(newVel);
		}

		void FSM::transformToEndpoints(Vector2 pos, float angle, sensor_msgs::LaserScan ls){
    			ROS_DEBUG("Convert laser scan to endpoints");
    			double start_angle = ls.angle_min;
    			double increment = ls.angle_increment;
    			
    			double r_x = pos._x;
    			double r_y = pos._y;
    			double r_ang = angle;

			ros::Time current_time;
  			current_time = ros::Time::now();
			geometry_msgs::PoseArray end;
			end.header.frame_id = "map";
			end.header.stamp = ros::Time::now();
    			
    			for(int i = 0 ; i < ls.ranges.size(); i++){
				geometry_msgs::Pose pose;
				pose.position.x = r_x + (ls.ranges[i] * cos(start_angle + r_ang));
				pose.position.y = r_y + (ls.ranges[i] * sin(start_angle + r_ang));
				end.poses.push_back(pose);
       				start_angle = start_angle + increment;
    			}
			_pub_endpoints.publish(end);
		}


		void FSM::computeRayScan( Agents::BaseAgent * agent, sensor_msgs::LaserScan& ls) {
			const size_t ID = agent->_id;
			// Evalute the new state's velocity
			
			Vector2 pos = agent->_pos;
			//In radians to represent a 220 degree scan with 1/3 degree increment for a total of 660 ray scans
			float start_angle = -1.91986; 
			float end_angle = 1.918;
			float increment = 0.005817;
			float range_max = 25; 
			//In meters 
			/* 
			for(int i = 0;i < 660;i++){
				ls.ranges.push_back(0);
			}

			// parallel implementati
			//std::cout << "Parallelization " << ls.ranges.size() << std::endl; 
			#pragma omp parallel for
			for(int i = 0; i < 660; i++){
				//std::cout << "Generating obstacle distance " << angle; 
				//for each angle compute the distance from the obstacle
				float angle = start_angle + (increment * i);
				float distance =  distanceFromObstacle(angle,range_max, agent);
				float distance_agent = distanceFromAgent(angle, range_max, agent);
				if(distance > distance_agent){
					distance = distance_agent;
				}
				ls.ranges[i] = distance;
				//std::cout << " distance : " << distance << std::endl; 
			}
			ls.angle_min = start_angle;
			ls.angle_max = end_angle;
			ls.angle_increment = increment;
			ls.range_max = range_max;
			*/
			// serial implementation
			
			for(float angle = start_angle; angle <= end_angle ; angle += increment){
				//std::cout << "Generating obstacle distance " << angle; 
				//for each angle compute the distance from the obstacle
				float distance =  distanceFromObstacle(angle,range_max, agent);
				float distance_agent = distanceFromAgent(angle, range_max, agent);
				if(distance > distance_agent){
					distance = distance_agent;
				}
				ls.ranges.push_back(distance);
				//std::cout << " distance : " << distance << std::endl; 
			}
			ls.angle_min = start_angle;
			ls.angle_max = end_angle;
			ls.angle_increment = increment;
			ls.range_max = range_max;
			//std::cout << i << std::endl; 
		}

		float FSM::distanceFromAgent(float angle, float range_max, Agents::BaseAgent * agent){
			// find the vector to represent the ray 
			float min_range = 0.01;
			float width = 0.1;
			
			float agent_dir_angle = atan2(agent->_orient._y, agent->_orient._x);
			//std::cout << "Calculate laser:Agent orient x:" << agent->_orient._x << " y:" << agent->_orient._y << std::endl;
			//std::cout << "Calculate Agent direction : " << agent_dir_angle << std::endl;
			float new_angle = agent_dir_angle + angle;
			//
			Vector2 laser_begin = agent->_pos;
			Vector2 laser_end = laser_begin;
			laser_end._x += cos(new_angle) * range_max;
			laser_end._y += sin(new_angle) * range_max;
			return nearAgentDistance(laser_begin, laser_end);
		}
	
		float FSM::distanceFromObstacle(float angle, float range_max, Agents::BaseAgent * agent){
			// find the vector to represent the ray 
			float min_range = 0.01;
			float width = 0.1;
			
			float agent_dir_angle = atan2(agent->_orient._y, agent->_orient._x);
			//std::cout << "Calculate laser:Agent orient x:" << agent->_orient._x << " y:" << agent->_orient._y << std::endl;
			//std::cout << "Calculate Agent direction : " << agent_dir_angle << std::endl;
			float new_angle = agent_dir_angle + angle;
			//
			Vector2 laser_begin = agent->_pos;
			Vector2 laser_end = laser_begin;
			laser_end._x += cos(new_angle) * range_max;
			laser_end._y += sin(new_angle) * range_max;
			Vector2 laser_mid;
			if(_sim->queryVisibility(laser_begin,laser_end, width)){
				return range_max;	
			}
			else{
				while(laser_begin.distance(laser_end) > min_range){
					laser_mid._x = (laser_begin._x + laser_end._x)/2;
					laser_mid._y = (laser_begin._y + laser_end._y)/2;
					if(_sim->queryVisibility(laser_begin,laser_mid, width)){
						laser_begin = laser_mid;
					}
					else{
						laser_end = laser_mid;
					}
				}
				return laser_end.distance(agent->_pos);
			}
		}

		//return true if start and end are visible to each other 
		float FSM::nearAgentDistance(Vector2 start, Vector2 end){
			float distance = start.distance(end);
			int agtCount = (int)this->_sim->getNumAgents();
			for ( int a = 0; a < agtCount; ++a ) {
				Agents::BaseAgent * agt = this->_sim->getAgent( a );
				if(agt->_isExternal == false){
					//the agent in question is a crowd agent
					Vector2 agent_pos = agt->_pos;
					float radius = agt->_radius * 1.1;
					float current = intersect(start, end, agent_pos, radius);
					if(current < distance) 
						distance = current;				
				}
			}
			return distance;
		}

		float FSM::intersect(Vector2 start, Vector2 end, Vector2 circle, float radius){
			double r = radius;
  			double cx = circle._x;
  			double cy = circle._y;
  			double m = (start._y - end._y)/(start._x - end._x);
  			double b = ((-1)*start._x * m) + start._y;
  			double A = (m*m) + 1;
  			double B = 2*(m*b - m*cy - cx);
  			double C = (cx*cx) + (b*b) + (cy*cy) - (2*b*cy) - (r*r);
  			//cout << r << " " << cx << " " << cy << " " << m << " " << b << " " << A << " " << B << " " << C << endl;
  			// solution of the equation of point of intersection of the line segment and circle
			if((B*B) - (4*A*C) > 0){
				Vector2 first,second;
  				first._x = (-B + sqrt( ((B*B) - (4*A*C)) ))/(2*A);
  				first._y = m*first._x + b;
  				second._x = (-B - sqrt( ((B*B) - (4*A*C)) ))/(2*A);
  				second._y = m*second._x + b;
				if(in_between(start,first,end) and in_between(start,second,end)){
					float d1 = start.distance(first);
					float d2 = start.distance(second);
					if(d1 < d2) return d1;
					else return d2;
				}
			}
			return start.distance(end);
  		}
		//returns true if point lies in between start and end points
		bool FSM::in_between(Vector2 start, Vector2 point, Vector2 end){
			float ac = start.distance(end);
			float ab = start.distance(point);
			float bc = point.distance(end);
			if((ab + bc - ac) < 0.01){
				return true;
			}
			else{
				return false;
			}
		}


		/////////////////////////////////////////////////////////////////////

		State * FSM::getNode( const std::string & name ) {
			const size_t STATE_COUNT = _nodes.size();
			for ( size_t i = 0; i < STATE_COUNT; ++i ) {
				if ( _nodes[i]->getName() == name ) {
					return _nodes[i];
				}
			}
			return 0x0;
		}

		/////////////////////////////////////////////////////////////////////

		size_t FSM::addNode( State *node ) {
			if ( _currNode[0] == 0x0 ) {
				for ( size_t i = 0; i < _agtCount; ++i ) {
					_currNode[i] = node;
				}
			}
			_nodes.push_back( node );
			return _nodes.size() - 1;
		}

		/////////////////////////////////////////////////////////////////////

		bool FSM::addTransition( size_t fromNode, Transition * t ) {
			if ( fromNode >= _nodes.size() ) return false;
			State * from = _nodes[ fromNode ];
			from->addTransition( t );
			return true;
		}

		/////////////////////////////////////////////////////////////////////

		bool FSM::addGoal( size_t goalSet, size_t goalID, Goal * goal ) {
			if ( _goalSets.find( goalSet ) == _goalSets.end() ) {
				_goalSets[ goalSet ] = new GoalSet();
			}
			return _goalSets[ goalSet ]->addGoal( goalID, goal );
		}

		/////////////////////////////////////////////////////////////////////

		const Goal * FSM::getGoal( size_t goalSet, size_t goalID ) {
			if ( _goalSets.find( goalSet ) == _goalSets.end() ) {
				return 0x0;
			}
			return _goalSets[ goalSet ]->getGoalByID( goalID );
		}

		/////////////////////////////////////////////////////////////////////

		const GoalSet * FSM::getGoalSet( size_t goalSetID ) {
			if ( _goalSets.find( goalSetID ) == _goalSets.end() ) {
				return 0x0;
			}
			return _goalSets[ goalSetID ];
		}
		 
		/////////////////////////////////////////////////////////////////////

		void FSM::setCurrentState( Agents::BaseAgent * agent, size_t currNode ) {
			assert( currNode < _nodes.size() && "Set invalid state as current state" );
			_currNode[ agent->_id ] = _nodes[ currNode ];
		}

		/////////////////////////////////////////////////////////////////////

		State * FSM::getCurrentState( const Agents::BaseAgent * agt ) const {
			return _currNode[ agt->_id ];
		}

		/////////////////////////////////////////////////////////////////////

		size_t FSM::getAgentStateID( const Agents::BaseAgent * agent ) const { 
			return _currNode[ agent->_id ]->getID(); 
		}

		/////////////////////////////////////////////////////////////////////

		size_t FSM::getAgentStateID( size_t agentID ) const { 
			return _currNode[ agentID ]->getID(); 
		}

		/////////////////////////////////////////////////////////////////////

		bool FSM::allFinal() const {
			// NOTE: This assumes that there are no holes in this memory
			for ( size_t a = 0; a < _agtCount; ++a ) {
				if ( !_currNode[ a ]->getFinal() ) return false;
			}
			return true;
		}

		/////////////////////////////////////////////////////////////////////

		bool FSM::doStep() {
			// NOTE: This is a cast from size_t to int to be compatible with older implementations
			//		of openmp which require signed integers as loop variables

			SIM_TIME = this->_sim->getGlobalTime();
			EVENT_SYSTEM->evaluateEvents();
			int agtCount = (int)this->_sim->getNumAgents();
			size_t exceptionCount = 0;
			ros::Time current_time;
  			current_time = ros::Time::now();
			geometry_msgs::PoseArray crowd;
			Vector2 robot_pos;
			Vector2 robot_orient;
			float robot_angle;
			// Compute preference velocities for each agent and also send the base_scan and robot position estimates
			#pragma omp parallel for reduction(+:exceptionCount)
			for ( int a = 0; a < agtCount; ++a ) {
				Agents::BaseAgent * agt = this->_sim->getAgent( a );
				try {
					advance( agt );
					this->computePrefVelocity( agt );
				} catch ( StateException & e ) {
					logger << Logger::ERR_MSG << e.what() << "\n";
					++exceptionCount;
				}
				geometry_msgs::Pose pose;
				
				pose.position.x = agt->_pos._x;
				pose.position.y = agt->_pos._y;
				pose.position.z = 0.0;

				pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, atan2(agt->_orient._y, agt->_orient._x));

				geometry_msgs::PoseStamped poseStamped;
				poseStamped.pose = pose;

				if(agt->_isExternal){
					robot_pos = agt->_pos;
					robot_orient = agt->_orient;
					robot_angle = atan2(agt->_orient._y, agt->_orient._x);
					//std::cout << agt->_id << std::endl;
					//std::cout <<"pos:" << agt->_pos._x << " " << agt->_pos._y << std::endl;
					//std::cout <<"vel:" << agt->_vel._x << " " << agt->_vel._y << std::endl;
					//std::cout <<"orient:" << agt->_orient._x << " " << agt->_orient._y << std::endl;
						
					tf::TransformBroadcaster broadcaster1;
					tf::TransformBroadcaster broadcaster2;

					broadcaster1.sendTransform(
      						tf::StampedTransform(
        					tf::Transform(tf::createIdentityQuaternion(), tf::Vector3(0.0, 0.0, 0.0)),
        					ros::Time::now() + ros::Duration(0),"map", "pose"));

					broadcaster2.sendTransform(
      						tf::StampedTransform(
        					tf::Transform(tf::createQuaternionFromRPY(0.0, 0.0, robot_angle),
						tf::Vector3(pose.position.x, pose.position.y, 0.0)),
        					ros::Time::now() + ros::Duration(0),"pose", "base_scan"));


					//std::cout << "Robot position : (" << pose.position.x << "," << pose.position.y << ")" << std::endl;  
					//std::cout << "Robot orientation : (" << agt->_orient._x << "," << agt->_orient._y << ")" << std::endl;  

					poseStamped.header.stamp = ros::Time::now();
					//Change it to odom frame for IMU measurements
    					poseStamped.header.frame_id = "map";
					_pub_pose.publish(poseStamped);
					//send laser sensor messages
					sensor_msgs::LaserScan ls;					
					this->computeRayScan(agt,ls);
					ls.header.stamp = ros::Time::now();
					ls.header.frame_id = "base_scan";
					_pub_scan.publish(ls);

					//publish endpoints for rviz display
					transformToEndpoints(robot_pos,robot_angle,ls);

				}
			}
			// Compute and publish the crowd positions that is visible to the robot via laser scan
			for ( int a = 0; a < agtCount; ++a ) {
				Agents::BaseAgent * agt = this->_sim->getAgent( a );
				Vector2 agent_pos = agt->_pos;
				Vector2 agent_orient = agt->_orient;
				if(_sim->queryVisibility(agent_pos,robot_pos, 0.01) and !agt->_isExternal){
					double dx = agent_pos._x - robot_pos._x;
					double dy = agent_pos._y - robot_pos._y;
					double distance = sqrt((dx * dx) + (dy * dy));

					// Angle between robot orientation vector and robot-agent vector 
					//double robot_ort = atan2(robot_orient._y, robot_orient._x);
					//double robot_agent_ort = atan2(dy, dx);
					//double difference = robot_ort - robot_agent_ort;
					
					double len_robot = sqrt(pow(robot_orient._x,2) + pow(robot_orient._y,2));
					double len_robot_agent = sqrt(pow(dx,2) + pow(dy,2));
					double dot_product=(robot_orient._x * dx) + (robot_orient._y * dy);
					
					double difference = acos(dot_product/(len_robot*len_robot_agent));
					if(difference > 6.283){ 
						difference = difference - 6.283;
					}
					else if(difference < -6.283){
						difference = difference + 6.283;
					}
					 
					if(distance < 25 and abs(difference) < 1.9198){
						geometry_msgs::Pose pose;
						pose.position.x = agent_pos._x;
						pose.position.y = agent_pos._y;
						pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, atan2(agt->_orient._y, agt->_orient._x));
						crowd.poses.push_back(pose);
					}
					/*geometry_msgs::Pose pose;
					pose.position.x = agent_pos._x;
					pose.position.y = agent_pos._y;
					pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, atan2(agt->_orient._y, agt->_orient._x));
					crowd.poses.push_back(pose);*/
				}
				/*geometry_msgs::Pose pose;
			    pose.position.x = agent_pos._x;
			    pose.position.y = agent_pos._y;
			    pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, atan2(agt->_orient._y, agt->_orient._x));
			    crowd.poses.push_back(pose);*/
			}
			
			

			crowd.header.stamp = current_time;
			crowd.header.frame_id = "map";
			_pub_crowd.publish(crowd);

			if ( exceptionCount > 0 ) {
				throw FSMFatalException();
			}
			return this->allFinal();
		}

		/////////////////////////////////////////////////////////////////////

		void FSM::doTasks() {
			for ( size_t i = 0; i < this->_tasks.size(); ++i ) {
				try {
					this->_tasks[i]->doWork( this );
				} catch ( TaskFatalException ) {
					logger << Logger::ERR_MSG << "Fatal error in FSM task: " << _tasks[i]->toString() << "\n";
					throw FSMFatalException();
				} catch ( TaskException ) {
					logger << Logger::ERR_MSG << "Error in FSM task: " << _tasks[i]->toString() << "\n";
				}
			}
		}

		/////////////////////////////////////////////////////////////////////

		void FSM::finalize() {
			EVENT_SYSTEM->finalize();
			doTasks();
		}

		/////////////////////////////////////////////////////////////////////

		FsmContext * FSM::getContext() {
			FsmContext * ctx = new FsmContext( this );
			// TODO: Populate the context
			for ( size_t i = 0; i < _nodes.size(); ++i ) {
				StateContext * sCtx = new StateContext( _nodes[ i ] );
				ctx->addStateContext( _nodes[i]->getID(), sCtx );
			}
			
			return ctx;
		}
	}	// namespace BFSM
}	// namespace Menge
