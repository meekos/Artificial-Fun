#include "ET_Bot.h"
#include "misc/Cgdi.h"
#include "misc/utils.h"
#include "2D/Transformations.h"
#include "2D/Geometry.h"
#include "../../lua/Raven_Scriptor.h"
#include "ET_BotScriptor.h"
#include "../../Raven_Game.h"
#include "../../navigation/Raven_PathPlanner.h"
#include "ET_BotSteeringBehaviors.h"
#include "../../Raven_UserOptions.h"
#include "time/Regulator.h"
#include "../RavenBot/Raven_WeaponSystem.h"
#include "../../Raven_SensoryMemory.h"

#include "Messaging/Telegram.h"
#include "../../Raven_Messages.h"
#include "Messaging/MessageDispatcher.h"
#include "game/EntityManager.h"

#include "../../goals/Raven_Goal_Types.h"
#include "ETGoal_Think.h"


#include "Debug/DebugConsole.h"

//-------------------------- ctor ---------------------------------------------
ET_Bot::ET_Bot(Raven_Game* world,Vector2D pos): AbstRaven_Bot(world, pos)       
{
	m_pScript = ET_BotScriptor::Instance();
	
	SetEntityType(type_bot);

  SetUpVertexBuffer();
  
  //a bot starts off facing in the direction it is heading
  m_vFacing = m_vHeading;

  //create the navigation module
  m_pPathPlanner = new Raven_PathPlanner(this);

  //create the steering behavior class
  m_pSteering = new ET_BotSteering(world, this);

  //create the regulators
  m_pWeaponSelectionRegulator = new Regulator(m_pScript->GetDouble("Bot_WeaponSelectionFrequency"));
  m_pGoalArbitrationRegulator =  new Regulator(m_pScript->GetDouble("Bot_GoalAppraisalUpdateFreq"));
  m_pTargetSelectionRegulator = new Regulator(m_pScript->GetDouble("Bot_TargetingUpdateFreq"));
  m_pTriggerTestRegulator = new Regulator(script->GetDouble("Bot_TriggerUpdateFreq"));
  m_pVisionUpdateRegulator = new Regulator(m_pScript->GetDouble("Bot_VisionUpdateFreq"));

  //create the goal queue
  m_pBrain = new ETGoal_Think(this);

  //create the targeting system
  m_pTargSys = new ET_TargetingSystem(this);

  m_pWeaponSys = new Raven_WeaponSystem(this,
                                        script->GetDouble("Bot_ReactionTime"),
                                        script->GetDouble("Bot_AimAccuracy"),
                                        m_pScript->GetDouble("Bot_AimPersistance"));

  m_pSensoryMem = new Raven_SensoryMemory(this, m_pScript->GetDouble("Bot_MemorySpan"));
}

//-------------------------------- dtor ---------------------------------------
//-----------------------------------------------------------------------------
ET_Bot::~ET_Bot()
{
  #ifdef SHOW_NAVINFO
    debug_con << "deleting ET bot (id = " << ID() << ")" << "";
  #endif 
  
  delete m_pBrain;
  delete m_pPathPlanner;
  delete m_pSteering;
  delete m_pWeaponSelectionRegulator;
  delete m_pTargSys;
  delete m_pGoalArbitrationRegulator;
  delete m_pTargetSelectionRegulator;
  delete m_pTriggerTestRegulator;
  delete m_pVisionUpdateRegulator;
  delete m_pWeaponSys;
  delete m_pSensoryMem;
}

//-------------------------------- Update -------------------------------------
//
void ET_Bot::DoUpdate()
{
  //process the currently active goal. Note this is required even if the bot
  //is under user control. This is because a goal is created whenever a user 
  //clicks on an area of the map that necessitates a path planning request.
  m_pBrain->Process();
  
  //Calculate the steering force and update the bot's velocity and position
  UpdateMovement();

  //if the bot is under AI control but not scripted
  if (!isPossessed())
  {           
    //examine all the opponents in the bots sensory memory and select one
    //to be the current target
    if (m_pTargetSelectionRegulator->isReady())
    {      
      m_pTargSys->Update();
    }

    //appraise and arbitrate between all possible high level goals
    if (m_pGoalArbitrationRegulator->isReady())
    {
       m_pBrain->Arbitrate(); 
    }

    //update the sensory memory with any visual stimulus
    if (m_pVisionUpdateRegulator->isReady())
    {
      m_pSensoryMem->UpdateVision();
    }
  
    //select the appropriate weapon to use from the weapons currently in
    //the inventory
    if (m_pWeaponSelectionRegulator->isReady())
    {       
      m_pWeaponSys->SelectWeapon();       
    }

    //this method aims the bot's current weapon at the current target
    //and takes a shot if a shot is possible
    m_pWeaponSys->TakeAimAndShoot();
  }
}



//--------------------------- HandleMessage -----------------------------------
//-----------------------------------------------------------------------------
bool ET_Bot::HandleMessage(const Telegram& msg)
{
	int damage = 0;
  //first see if the current goal accepts the message
  if (GetBrain()->HandleMessage(msg)) return true;
 
  //handle any messages not handles by the goals
  switch(msg.Msg)
  {
  case Msg_TakeThatMF:

    //just return if already dead or spawning
    if (isDead() || isSpawning()) return true;
	else
	{

		damage = DereferenceToType<int>(msg.ExtraInfo);

		//the extra info field of the telegram carries the amount of damage
		ReduceHealth(damage);

		AbstRaven_Bot* pShooter = (AbstRaven_Bot*)EntityManager::Instance()->GetEntityFromID(msg.Sender);
		GetSensoryMem()->UpdateWithDamageSource(pShooter, damage);	

		//if this bot is now dead let the shooter know
		if (isDead())
		{
			Dispatcher->DispatchMsg(SEND_MSG_IMMEDIATELY,
                              ID(),
                              msg.Sender,
                              Msg_YouGotMeYouSOB,
                              NO_ADDITIONAL_INFO);
		}
	}

    return true;

  case Msg_YouGotMeYouSOB:
    
    IncrementScore();
    
    //the bot this bot has just killed should be removed as the target
    m_pTargSys->ClearTarget();

    return true;

  case Msg_GunshotSound:

    //add the source of this sound to the bot's percepts
    GetSensoryMem()->UpdateWithSoundSource((AbstRaven_Bot*)msg.ExtraInfo);

    return true;

  case Msg_UserHasRemovedBot:
    {

      AbstRaven_Bot* pRemovedBot = (AbstRaven_Bot*)msg.ExtraInfo;

      GetSensoryMem()->RemoveBotFromMemory(pRemovedBot);

      //if the removed bot is the target, make sure the target is cleared
      if (pRemovedBot == GetTargetSys()->GetTarget())
      {
        GetTargetSys()->ClearTarget();
      }

      return true;
    }


  default: return false;
  }
}


void			ET_Bot::SelectBodyPen()
{
	gdi->GreenPen();
}
void			ET_Bot::SelectHeadPen()
 {
	 gdi->BluePen();
 }

std::string const ET_Bot::GetName () const { return "ET_Bot"; }


