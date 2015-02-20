;---------------------------------------------------------------------------
;  order.clp - RoCKIn RefBox CLIPS - order specification
;
;  Licensed under BSD license, cf. LICENSE file
;---------------------------------------------------------------------------

(defclass Order (is-a USER) (role concrete)
  (slot id (type INTEGER))
  (slot status (type SYMBOL) (allowed-values OFFERED TIMEOUT IN_PROGRESS PAUSED ABORTED FINISHED))
  (slot object-id (type INTEGER))                             ; id of an object identifier
  (multislot container-id (type INTEGER) (cardinality 0 1))   ; id of an object identifier
  (slot quantity-delivered (type INTEGER) (default 0))
  (multislot quantity-requested (type INTEGER) (cardinality 0 1))
  (multislot destination-id (type INTEGER) (cardinality 0 1))  ; id of a location identifier
  (multislot source-id (type INTEGER) (cardinality 0 1))       ; id of a location identifier
  (multislot processing-team (type STRING) (cardinality 0 1))
)

(defmessage-handler Order create-msg ()
  "Create a ProtoBuf message of an order"

  (bind ?o (pb-create "rockin_msgs.Order"))

  (pb-set-field ?o "id" ?self:id)
  (pb-set-field ?o "status" ?self:status)

  (bind ?oi (net-create-ObjectIdentifier ?self:object-id))
  (pb-set-field ?o "object" ?oi)  ; destroys ?oi

  (if (<> (length$ ?self:container-id) 0) then
    (bind ?ci (net-create-ObjectIdentifier (nth$ 1 ?self:container-id)))
    (pb-set-field ?o "container" ?ci)  ; destroys ?ci
  )

  (pb-set-field ?o "quantity_delivered" ?self:quantity-delivered)

  (if (<> (length$ ?self:quantity-requested) 0) then
    (pb-set-field ?o "quantity_requested" (nth$ 1 ?self:quantity-requested))
  )

  (if (<> (length$ ?self:destination-id) 0) then
    (bind ?li (net-create-LocationIdentifier (nth$ 1 ?self:destination-id)))
    (pb-set-field ?o "destination" ?li) ; destroys ?li
  )

  (if (<> (length$ ?self:source-id) 0) then
    (bind ?si (net-create-LocationIdentifier (nth$ 1 ?self:source-id)))
    (pb-set-field ?o "source" ?si) ; destroys ?si
  )

  (if (<> (length$ ?self:processing-team) 0) then
    (pb-set-field ?o "processing_team" (nth$ 1 ?self:processing-team))
  )

  (return ?o)
)