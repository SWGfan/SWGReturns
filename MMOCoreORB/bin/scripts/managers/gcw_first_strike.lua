-- gcw_first_strike.lua
-- First Strike and Disguise system

gcwFirstStrike = {}

function gcwFirstStrike:canFirstStrike(player, target)
	if player == nil or target == nil then
		return false
	end

	if not SceneObject(player):isPlayerCreature() then
		return false
	end

	if not AiAgent(target):isAiAgent() then
		return false
	end

	local alertState = target:getCustomVariable("gcw_alert_state")
	if alertState == nil then alertState = "normal" end
	if alertState == "alerted" then
		return false
	end

	if isInGCWZone(player) == false then
		return false
	end

	if calculateAIDetection(target, player) > 0 then
		return false
	end

	return true
end

function gcwFirstStrike:performFirstStrike(player, target)
	if not gcwFirstStrike:canFirstStrike(player, target) then
		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.firstStrikeFail)
		return false
	end

	local maxHAM = AiAgent(target):getMaxHAM(0)
	if maxHAM > 0 then
		AiAgent(target):inflictDamage(target, 0, maxHAM, true)
	end

	CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.firstStrikeSuccess)

	return true
end

function gcwFirstStrike:canDisguise(player, npc)
	if player == nil or npc == nil then
		return false
	end

	if not AiAgent(npc):isDead() then
		return false
	end

	local templateName = SceneObject(npc):getTemplateObjectPath()
	if templateName == nil or templateName == "" then
		return false
	end

	local allowed = false
	if string.find(templateName, "stormtrooper") or string.find(templateName, "imperial") or string.find(templateName, "rebel") then
		allowed = true
	end

	if not allowed then
		return false
	end

	if isInGCWZone(player) == false then
		return false
	end

	return true
end

function gcwFirstStrike:applyDisguise(player, npc)
	if not gcwFirstStrike:canDisguise(player, npc) then
		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.disguiseFailed)
		return false
	end

	local disguiseTemplate = SceneObject(npc):getTemplateObjectPath()
	disguiseSystem:applyDisguise(player, disguiseTemplate)

	return true
end

function gcwFirstStrike:removeDisguise(player)
	disguiseSystem:removeDisguise(player)
end

function gcwFirstStrike:isDisguised(player)
	return disguiseSystem:isDisguised(player)
end

function gcwFirstStrike:firstStrikeRadial(player, target)
	if gcwFirstStrike:canFirstStrike(player, target) then
		gcwFirstStrike:performFirstStrike(player, target)
	else
		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.firstStrikeFail)
	end
end

function gcwFirstStrike:disguiseRadial(player, target)
	if gcwFirstStrike:canDisguise(player, target) then
		gcwFirstStrike:applyDisguise(player, target)
	else
		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.disguiseFailed)
	end
end

function gcwFirstStrike:checkDisguiseExpiration(player)
	if player == nil then return end
	if not gcwFirstStrike:isDisguised(player) then return end

	if isInGCWZone(player) == false then
		gcwFirstStrike:removeDisguise(player)
		return
	end
end
