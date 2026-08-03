-- Jenkin's Cloner (2026-07-29, Nick's feature ask) -- a house-placeable
-- personal clone point. Reuses the real, existing cloning-terminal console
-- model (object/tangible/terminal/shared_terminal_cloning.iff) for its
-- appearance; the actual clone-bind/relocate logic lives in
-- JenkinsClonerMenuComponent.h and PlayerManagerImplementation.cpp.

object_tangible_terminal_jenkins_cloner = object_tangible_terminal_shared_terminal_cloning:new {
	maxCondition = 1000,
	objectMenuComponent = "JenkinsClonerMenuComponent",
	objectName = "Jenkin's Cloner",
}

ObjectTemplates:addTemplate(object_tangible_terminal_jenkins_cloner, "object/tangible/terminal/jenkins_cloner.iff")
